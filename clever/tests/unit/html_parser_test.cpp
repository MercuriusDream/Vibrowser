#include <clever/html/tokenizer.h>
#include <clever/html/tree_builder.h>
#include <clever/dom/document.h>
#include <clever/dom/element.h>
#include <clever/dom/text.h>
#include <clever/dom/comment.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace clever::html;

// Helper: collect all tokens from input
static std::vector<Token> tokenize_all(std::string_view input) {
    Tokenizer tok(input);
    std::vector<Token> tokens;
    while (true) {
        auto t = tok.next_token();
        tokens.push_back(t);
        if (t.type == Token::EndOfFile) break;
    }
    return tokens;
}

// ============================================================================
// Tokenizer Tests
// ============================================================================

// 1. Simple start tag
TEST(Tokenizer, SimpleStartTag) {
    Tokenizer tok("<div>");
    auto t = tok.next_token();
    EXPECT_EQ(t.type, Token::StartTag);
    EXPECT_EQ(t.name, "div");
    EXPECT_FALSE(t.self_closing);
}

// 2. End tag
TEST(Tokenizer, EndTag) {
    Tokenizer tok("</div>");
    auto t = tok.next_token();
    EXPECT_EQ(t.type, Token::EndTag);
    EXPECT_EQ(t.name, "div");
}

// 3. Self-closing tag
TEST(Tokenizer, SelfClosingTag) {
    Tokenizer tok("<br/>");
    auto t = tok.next_token();
    EXPECT_EQ(t.type, Token::StartTag);
    EXPECT_EQ(t.name, "br");
    EXPECT_TRUE(t.self_closing);
}

// 4. Tag with attributes
TEST(Tokenizer, TagWithAttributes) {
    Tokenizer tok(R"(<a href="url" class="link">)");
    auto t = tok.next_token();
    EXPECT_EQ(t.type, Token::StartTag);
    EXPECT_EQ(t.name, "a");
    ASSERT_EQ(t.attributes.size(), 2u);
    EXPECT_EQ(t.attributes[0].name, "href");
    EXPECT_EQ(t.attributes[0].value, "url");
    EXPECT_EQ(t.attributes[1].name, "class");
    EXPECT_EQ(t.attributes[1].value, "link");
}

// 5. Text content
TEST(Tokenizer, TextContent) {
    auto tokens = tokenize_all("Hello World");
    // Should produce Character tokens (possibly one per char or batched)
    // then EOF. Verify combined text = "Hello World"
    std::string text;
    for (auto& t : tokens) {
        if (t.type == Token::Character) text += t.data;
    }
    EXPECT_EQ(text, "Hello World");
}

// 6. Comment
TEST(Tokenizer, Comment) {
    Tokenizer tok("<!-- comment -->");
    auto t = tok.next_token();
    EXPECT_EQ(t.type, Token::Comment);
    EXPECT_EQ(t.data, " comment ");
}

// 7. DOCTYPE
TEST(Tokenizer, DOCTYPE) {
    Tokenizer tok("<!DOCTYPE html>");
    auto t = tok.next_token();
    EXPECT_EQ(t.type, Token::DOCTYPE);
    EXPECT_EQ(t.name, "html");
    EXPECT_FALSE(t.force_quirks);
}

// 8. Mixed content
TEST(Tokenizer, MixedContent) {
    auto tokens = tokenize_all("<p>Hello</p>");
    // Expect: StartTag "p", Character(s) "Hello", EndTag "p", EOF
    ASSERT_GE(tokens.size(), 4u);  // at least start, char(s), end, eof
    EXPECT_EQ(tokens.front().type, Token::StartTag);
    EXPECT_EQ(tokens.front().name, "p");

    std::string text;
    for (auto& t : tokens) {
        if (t.type == Token::Character) text += t.data;
    }
    EXPECT_EQ(text, "Hello");

    // Find the EndTag
    auto it = std::find_if(tokens.begin(), tokens.end(),
        [](const Token& t) { return t.type == Token::EndTag; });
    ASSERT_NE(it, tokens.end());
    EXPECT_EQ(it->name, "p");

    EXPECT_EQ(tokens.back().type, Token::EndOfFile);
}

// 9. Nested tags
TEST(Tokenizer, NestedTags) {
    auto tokens = tokenize_all("<div><p>text</p></div>");
    std::vector<std::string> tag_names;
    for (auto& t : tokens) {
        if (t.type == Token::StartTag || t.type == Token::EndTag) {
            tag_names.push_back((t.type == Token::StartTag ? "+" : "-") + t.name);
        }
    }
    ASSERT_EQ(tag_names.size(), 4u);
    EXPECT_EQ(tag_names[0], "+div");
    EXPECT_EQ(tag_names[1], "+p");
    EXPECT_EQ(tag_names[2], "-p");
    EXPECT_EQ(tag_names[3], "-div");
}

// 10. Multiple attributes with various quotes
TEST(Tokenizer, MultipleAttributesWithQuotes) {
    Tokenizer tok(R"(<div id="main" class='container' data-x="y">)");
    auto t = tok.next_token();
    EXPECT_EQ(t.type, Token::StartTag);
    EXPECT_EQ(t.name, "div");
    ASSERT_EQ(t.attributes.size(), 3u);
    EXPECT_EQ(t.attributes[0].name, "id");
    EXPECT_EQ(t.attributes[0].value, "main");
    EXPECT_EQ(t.attributes[1].name, "class");
    EXPECT_EQ(t.attributes[1].value, "container");
    EXPECT_EQ(t.attributes[2].name, "data-x");
    EXPECT_EQ(t.attributes[2].value, "y");
}

// 11. Unquoted attribute value
TEST(Tokenizer, UnquotedAttributeValue) {
    Tokenizer tok("<div class=foo>");
    auto t = tok.next_token();
    EXPECT_EQ(t.type, Token::StartTag);
    ASSERT_EQ(t.attributes.size(), 1u);
    EXPECT_EQ(t.attributes[0].name, "class");
    EXPECT_EQ(t.attributes[0].value, "foo");
}

// 12. Attribute without value
TEST(Tokenizer, AttributeWithoutValue) {
    Tokenizer tok("<input disabled>");
    auto t = tok.next_token();
    EXPECT_EQ(t.type, Token::StartTag);
    EXPECT_EQ(t.name, "input");
    ASSERT_EQ(t.attributes.size(), 1u);
    EXPECT_EQ(t.attributes[0].name, "disabled");
    EXPECT_EQ(t.attributes[0].value, "");
}

// 13. Script tag state switching
TEST(Tokenizer, ScriptTagStateSwitching) {
    // When in ScriptData state, content is treated as raw text
    // until the matching </script> end tag
    Tokenizer tok("var x = 1;</script>");
    tok.set_state(TokenizerState::ScriptData);
    tok.set_last_start_tag("script");

    std::string script_text;
    std::vector<Token> tokens;
    while (true) {
        auto t = tok.next_token();
        tokens.push_back(t);
        if (t.type == Token::Character) script_text += t.data;
        if (t.type == Token::EndOfFile || t.type == Token::EndTag) break;
    }
    EXPECT_EQ(script_text, "var x = 1;");
    // Last non-EOF token should be EndTag "script"
    auto it = std::find_if(tokens.begin(), tokens.end(),
        [](const Token& t) { return t.type == Token::EndTag; });
    ASSERT_NE(it, tokens.end());
    EXPECT_EQ(it->name, "script");
}

// ============================================================================
// TreeBuilder Tests
// ============================================================================

// 14. Basic complete document
TEST(TreeBuilder, BasicDocument) {
    auto doc = parse("<html><head><title>Test</title></head><body><p>Hello</p></body></html>");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->type, SimpleNode::Document);

    auto* html = doc->find_element("html");
    ASSERT_NE(html, nullptr);

    auto* head = doc->find_element("head");
    ASSERT_NE(head, nullptr);

    auto* title = doc->find_element("title");
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->text_content(), "Test");

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hello");
}

// 15. Missing html/head/body -- auto-generated
TEST(TreeBuilder, AutoGeneratedElements) {
    auto doc = parse("<p>Hello</p>");
    ASSERT_NE(doc, nullptr);

    auto* html = doc->find_element("html");
    ASSERT_NE(html, nullptr);

    auto* head = doc->find_element("head");
    ASSERT_NE(head, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hello");
}

// 16. Implicit closing: <p>One<p>Two => two separate p elements
TEST(TreeBuilder, ImplicitParagraphClosing) {
    auto doc = parse("<p>One<p>Two");
    ASSERT_NE(doc, nullptr);

    auto ps = doc->find_all_elements("p");
    ASSERT_EQ(ps.size(), 2u);
    EXPECT_EQ(ps[0]->text_content(), "One");
    EXPECT_EQ(ps[1]->text_content(), "Two");
}

// 17. Nested divs
TEST(TreeBuilder, NestedDivs) {
    auto doc = parse("<div><div>inner</div></div>");
    ASSERT_NE(doc, nullptr);

    auto divs = doc->find_all_elements("div");
    ASSERT_EQ(divs.size(), 2u);
    // The inner div should be a child of the outer div
    EXPECT_EQ(divs[1]->parent, divs[0]);
    EXPECT_EQ(divs[1]->text_content(), "inner");
}

// 18. Text in body
TEST(TreeBuilder, TextInBody) {
    auto doc = parse("Just some text");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->text_content(), "Just some text");
}

// 19. Void elements
TEST(TreeBuilder, VoidElements) {
    auto doc = parse("<br><img><hr><input>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    auto* br = doc->find_element("br");
    ASSERT_NE(br, nullptr);
    EXPECT_TRUE(br->children.empty());

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_TRUE(img->children.empty());

    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
    EXPECT_TRUE(hr->children.empty());

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_TRUE(input->children.empty());
}

// 20. Unknown tags as normal elements
TEST(TreeBuilder, UnknownTags) {
    auto doc = parse("<mywidget>content</mywidget>");
    ASSERT_NE(doc, nullptr);

    auto* widget = doc->find_element("mywidget");
    ASSERT_NE(widget, nullptr);
    EXPECT_EQ(widget->text_content(), "content");
}

// 21. Whitespace handling in head vs body
TEST(TreeBuilder, WhitespaceHandling) {
    auto doc = parse("<html> <head> </head> <body> text </body> </html>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    // Body should contain the text (with whitespace)
    std::string content = body->text_content();
    EXPECT_NE(content.find("text"), std::string::npos);
}

// 22. Comments
TEST(TreeBuilder, Comments) {
    auto doc = parse("<!-- comment --><html><body>text</body></html>");
    ASSERT_NE(doc, nullptr);

    // Document should have a comment child
    bool found_comment = false;
    for (auto& child : doc->children) {
        if (child->type == SimpleNode::Comment) {
            found_comment = true;
            EXPECT_EQ(child->data, " comment ");
        }
    }
    EXPECT_TRUE(found_comment);
}

// 23. DOCTYPE
TEST(TreeBuilder, DocType) {
    auto doc = parse("<!DOCTYPE html><html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    bool found_doctype = false;
    for (auto& child : doc->children) {
        if (child->type == SimpleNode::DocumentType) {
            found_doctype = true;
            EXPECT_EQ(child->doctype_name, "html");
        }
    }
    EXPECT_TRUE(found_doctype);
}

// 24. Heading elements
TEST(TreeBuilder, HeadingElements) {
    auto doc = parse("<h1>Title</h1>");
    ASSERT_NE(doc, nullptr);

    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Title");
}

// 25. Lists
TEST(TreeBuilder, Lists) {
    auto doc = parse("<ul><li>Item 1<li>Item 2</ul>");
    ASSERT_NE(doc, nullptr);

    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);

    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 2u);
    EXPECT_EQ(lis[0]->text_content(), "Item 1");
    EXPECT_EQ(lis[1]->text_content(), "Item 2");
}

// 26. parse() convenience function
TEST(TreeBuilder, ParseConvenience) {
    auto doc = parse("<div>test</div>");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->type, SimpleNode::Document);
    EXPECT_NE(doc->find_element("div"), nullptr);
}

TEST(TreeBuilder, ConvertSimpleToDomDocument) {
    auto simple_doc = parse("<!DOCTYPE html><html><head><title>Title</title></head>"
                           "<body><p id=\"intro\" class=\"hero\">Hello</p><!-- c --></body></html>");
    ASSERT_NE(simple_doc, nullptr);

    auto dom_doc = to_dom_document(*simple_doc);
    ASSERT_NE(dom_doc, nullptr);

    auto* html = dom_doc->document_element();
    ASSERT_NE(html, nullptr);
    EXPECT_EQ(html->tag_name(), "html");

    auto* head = dom_doc->head();
    ASSERT_NE(head, nullptr);
    EXPECT_EQ(head->tag_name(), "head");

    auto* body = dom_doc->body();
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->tag_name(), "body");

    auto* intro = dom_doc->get_element_by_id("intro");
    ASSERT_NE(intro, nullptr);
    EXPECT_EQ(intro->tag_name(), "p");
    EXPECT_EQ(intro->text_content(), "Hello");

    bool found_comment = false;
    body->for_each_child([&](const clever::dom::Node& child) {
        if (child.node_type() == clever::dom::NodeType::Comment) {
            found_comment = true;
            const auto& comment = static_cast<const clever::dom::Comment&>(child);
            EXPECT_EQ(comment.data(), " c ");
        }
    });
    EXPECT_TRUE(found_comment);
}

TEST(TreeBuilder, ConvertDomToSimpleNode) {
    auto dom_doc = std::make_unique<clever::dom::Document>();
    auto html = dom_doc->create_element("html");
    auto head = dom_doc->create_element("head");
    auto body = dom_doc->create_element("body");
    auto title = dom_doc->create_element("title");
    title->append_child(dom_doc->create_text_node("Title"));
    auto para = dom_doc->create_element("p");
    para->set_attribute("id", "main");
    para->append_child(dom_doc->create_text_node("Hi"));
    dom_doc->append_child(std::move(html));
    auto* html_ptr = dom_doc->document_element();
    html_ptr->append_child(std::move(head));
    html_ptr->append_child(std::move(body));
    auto* body_ptr = dom_doc->body();
    ASSERT_NE(body_ptr, nullptr);
    body_ptr->append_child(std::move(title));
    body_ptr->append_child(std::move(para));

    auto simple_doc = to_simple_node(*dom_doc);
    ASSERT_NE(simple_doc, nullptr);
    EXPECT_EQ(simple_doc->type, SimpleNode::Document);

    auto* para_node = simple_doc->find_element("p");
    ASSERT_NE(para_node, nullptr);
    EXPECT_EQ(para_node->tag_name, "p");
    EXPECT_EQ(para_node->text_content(), "Hi");
    EXPECT_EQ(para_node->attributes.size(), 1u);
    EXPECT_EQ(para_node->attributes[0].name, "id");
    EXPECT_EQ(para_node->attributes[0].value, "main");
}

// ============================================================================
// Full Pipeline Tests
// ============================================================================

// 27. Parse complex HTML and verify tree structure
TEST(FullPipeline, ComplexStructure) {
    auto doc = parse(R"(<div class="main"><p>Hello <em>World</em></p></div>)");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    ASSERT_EQ(div->attributes.size(), 1u);
    EXPECT_EQ(div->attributes[0].name, "class");
    EXPECT_EQ(div->attributes[0].value, "main");

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->parent, div);

    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->parent, p);
    EXPECT_EQ(em->text_content(), "World");

    // Full text of p should be "Hello World"
    EXPECT_EQ(p->text_content(), "Hello World");
}

// 28. Script/style tags (raw text)
TEST(FullPipeline, ScriptAndStyleTags) {
    auto doc = parse("<html><head><style>body { color: red; }</style></head>"
                     "<body><script>var x = 1 < 2;</script></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* style = doc->find_element("style");
    ASSERT_NE(style, nullptr);
    EXPECT_EQ(style->text_content(), "body { color: red; }");

    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    EXPECT_EQ(script->text_content(), "var x = 1 < 2;");
}

// 29. Malformed HTML: <b><i>text</b></i> -- should produce valid tree
TEST(FullPipeline, MalformedHTML) {
    auto doc = parse("<b><i>text</b></i>");
    ASSERT_NE(doc, nullptr);

    auto* b = doc->find_element("b");
    ASSERT_NE(b, nullptr);

    auto* i = doc->find_element("i");
    ASSERT_NE(i, nullptr);

    // The tree should still contain the text
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    std::string content = body->text_content();
    EXPECT_EQ(content, "text");
}

// ============================================================================
// HTML Entity Decoding
// ============================================================================

TEST(HtmlEntity, NamedEntitiesBasic) {
    auto tokens = tokenize_all("&amp; &lt; &gt; &quot; &apos;");
    std::string text;
    for (auto& t : tokens) {
        if (t.type == Token::Character) text += t.data;
    }
    EXPECT_EQ(text, "& < > \" '");
}

TEST(HtmlEntity, NamedEntitiesNbsp) {
    auto tokens = tokenize_all("hello&nbsp;world");
    std::string text;
    for (auto& t : tokens) {
        if (t.type == Token::Character) text += t.data;
    }
    // nbsp is U+00A0, encoded as 0xC2 0xA0 in UTF-8
    EXPECT_EQ(text, "hello\xC2\xA0world");
}

TEST(HtmlEntity, NumericDecimal) {
    auto tokens = tokenize_all("&#65;&#66;&#67;");
    std::string text;
    for (auto& t : tokens) {
        if (t.type == Token::Character) text += t.data;
    }
    EXPECT_EQ(text, "ABC");
}

TEST(HtmlEntity, NumericHex) {
    auto tokens = tokenize_all("&#x41;&#x42;&#x43;");
    std::string text;
    for (auto& t : tokens) {
        if (t.type == Token::Character) text += t.data;
    }
    EXPECT_EQ(text, "ABC");
}

TEST(HtmlEntity, UnicodeEntity) {
    // U+2764 HEAVY BLACK HEART = ❤ = 0xE2 0x9D 0xA4
    auto tokens = tokenize_all("&#x2764;");
    std::string text;
    for (auto& t : tokens) {
        if (t.type == Token::Character) text += t.data;
    }
    EXPECT_EQ(text, "\xE2\x9D\xA4");
}

TEST(HtmlEntity, InAttributeValue) {
    auto tokens = tokenize_all(R"(<a href="page?a=1&amp;b=2">)");
    for (auto& t : tokens) {
        if (t.type == Token::StartTag && t.name == "a") {
            ASSERT_FALSE(t.attributes.empty());
            EXPECT_EQ(t.attributes[0].value, "page?a=1&b=2");
            return;
        }
    }
    FAIL() << "No start tag found";
}

TEST(HtmlEntity, UnknownEntityPassthrough) {
    auto tokens = tokenize_all("&bogus; text");
    std::string text;
    for (auto& t : tokens) {
        if (t.type == Token::Character) text += t.data;
    }
    // Unknown entity should pass through as "&bogus; text" or just "&" + rest
    EXPECT_NE(text.find("&"), std::string::npos) << "Unknown entity should preserve &";
}

TEST(HtmlEntity, SpecialSymbols) {
    auto tokens = tokenize_all("&copy; &trade; &hellip; &mdash;");
    std::string text;
    for (auto& t : tokens) {
        if (t.type == Token::Character) text += t.data;
    }
    EXPECT_NE(text.find("\xC2\xA9"), std::string::npos) << "Should contain copyright symbol";
    EXPECT_NE(text.find("\xE2\x84\xA2"), std::string::npos) << "Should contain trademark symbol";
    EXPECT_NE(text.find("\xE2\x80\xA6"), std::string::npos) << "Should contain ellipsis";
    EXPECT_NE(text.find("\xE2\x80\x94"), std::string::npos) << "Should contain em dash";
}

TEST(HtmlEntity, TreeBuilderEntity) {
    auto doc = parse("<html><body><p>&lt;div&gt; &amp; &quot;hello&quot;</p></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "<div> & \"hello\"");
}

// ============================================================================
// Cycle 426: HTML parser structural regression tests
// ============================================================================

TEST(TreeBuilder, TableStructure) {
    auto doc = parse("<table><tr><td>cell1</td><td>cell2</td></tr></table>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);

    auto* tr = doc->find_element("tr");
    ASSERT_NE(tr, nullptr);

    auto* td = doc->find_element("td");
    ASSERT_NE(td, nullptr);
    EXPECT_EQ(td->text_content(), "cell1");
}

TEST(TreeBuilder, AnchorAttributes) {
    auto doc = parse("<a href=\"https://example.com\" target=\"_blank\">link</a>");
    ASSERT_NE(doc, nullptr);

    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);

    // Verify href attribute is preserved
    bool found_href = false;
    for (const auto& attr : a->attributes) {
        if (attr.name == "href") {
            found_href = true;
            EXPECT_EQ(attr.value, "https://example.com");
        }
    }
    EXPECT_TRUE(found_href);
}

TEST(TreeBuilder, SemanticElements) {
    auto doc = parse("<header><nav>nav</nav></header><main><article>content</article></main><footer>foot</footer>");
    ASSERT_NE(doc, nullptr);

    EXPECT_NE(doc->find_element("header"), nullptr);
    EXPECT_NE(doc->find_element("nav"), nullptr);
    EXPECT_NE(doc->find_element("main"), nullptr);
    EXPECT_NE(doc->find_element("article"), nullptr);
    EXPECT_NE(doc->find_element("footer"), nullptr);
}

TEST(TreeBuilder, UpperCaseTagsNormalized) {
    auto doc = parse("<DIV><P>text</P></DIV>");
    ASSERT_NE(doc, nullptr);

    // HTML5 requires tag names to be lowercased
    auto* div = doc->find_element("div");
    EXPECT_NE(div, nullptr);

    auto* p = doc->find_element("p");
    EXPECT_NE(p, nullptr);
}

TEST(TreeBuilder, FormElements) {
    auto doc = parse("<form><input type=\"text\" name=\"q\"><button type=\"submit\">Go</button></form>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);

    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text_content(), "Go");
}

TEST(TreeBuilder, EmptyDocument) {
    auto doc = parse("");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->type, SimpleNode::Document);
}

TEST(TreeBuilder, UnclosedElementRecovery) {
    // Tree builder should create the element even when the closing tag is absent
    auto doc = parse("<div>text without closing tag");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->text_content(), "text without closing tag");
}

TEST(HtmlEntity, MalformedEntityPassthrough) {
    // A bare '&' not followed by valid entity should pass through as-is
    auto tokens = tokenize_all("a & b");
    std::string text;
    for (auto& t : tokens) {
        if (t.type == Token::Character) text += t.data;
    }
    // The ampersand and surrounding text should be present
    EXPECT_NE(text.find("&"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Cycle 443 — HTML parser: nested lists, data attributes, multiple comments,
//             deeply nested structure, form with method/action, table cells,
//             textarea default text, select/option hierarchy
// ---------------------------------------------------------------------------

TEST(TreeBuilder, NestedOrderedAndUnorderedLists) {
    auto doc = parse("<ul><li>item1</li><li><ol><li>nested</li></ol></li></ul>");
    ASSERT_NE(doc, nullptr);

    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);

    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);

    auto* nested_li = doc->find_element("ol");
    ASSERT_NE(nested_li, nullptr);
    auto* inner_li = nested_li->find_element("li");
    ASSERT_NE(inner_li, nullptr);
    EXPECT_EQ(inner_li->text_content(), "nested");
}

TEST(TreeBuilder, DataAttributes) {
    auto doc = parse("<div data-user-id=\"42\" data-role=\"admin\">content</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);

    std::string data_user_id, data_role;
    for (const auto& attr : div->attributes) {
        if (attr.name == "data-user-id") data_user_id = attr.value;
        if (attr.name == "data-role") data_role = attr.value;
    }
    EXPECT_EQ(data_user_id, "42");
    EXPECT_EQ(data_role, "admin");
}

TEST(TreeBuilder, MultipleComments) {
    auto doc = parse("<!-- first --><!-- second --><div>x</div><!-- third -->");
    ASSERT_NE(doc, nullptr);

    // At least one comment node should be present; div must still be accessible
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->text_content(), "x");
}

TEST(TreeBuilder, DeeplyNestedDivs) {
    // 6 levels deep
    auto doc = parse("<div><div><div><div><div><div>deep</div></div></div></div></div></div>");
    ASSERT_NE(doc, nullptr);

    // find_element traverses the subtree; find "div" should succeed
    auto* outer = doc->find_element("div");
    ASSERT_NE(outer, nullptr);
    // Recursively look for the text "deep"
    EXPECT_NE(doc->find_element("div"), nullptr);
}

TEST(TreeBuilder, FormWithMethodAndAction) {
    auto doc = parse("<form method=\"post\" action=\"/submit\"><input name=\"q\"></form>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);

    std::string method_val, action_val;
    for (const auto& attr : form->attributes) {
        if (attr.name == "method") method_val = attr.value;
        if (attr.name == "action") action_val = attr.value;
    }
    EXPECT_EQ(method_val, "post");
    EXPECT_EQ(action_val, "/submit");

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    std::string name_val;
    for (const auto& attr : input->attributes) {
        if (attr.name == "name") name_val = attr.value;
    }
    EXPECT_EQ(name_val, "q");
}

TEST(TreeBuilder, TableWithCells) {
    auto doc = parse("<table><tr><td>A</td><td>B</td></tr><tr><td>C</td></tr></table>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);

    auto* td = doc->find_element("td");
    ASSERT_NE(td, nullptr);
    EXPECT_EQ(td->text_content(), "A");
}

TEST(TreeBuilder, TextareaDefaultText) {
    auto doc = parse("<textarea>Hello World</textarea>");
    ASSERT_NE(doc, nullptr);

    auto* ta = doc->find_element("textarea");
    ASSERT_NE(ta, nullptr);
    EXPECT_EQ(ta->text_content(), "Hello World");
}

TEST(TreeBuilder, SelectWithOptions) {
    auto doc = parse("<select name=\"color\"><option value=\"red\">Red</option><option value=\"blue\" selected>Blue</option></select>");
    ASSERT_NE(doc, nullptr);

    auto* select = doc->find_element("select");
    ASSERT_NE(select, nullptr);
    std::string select_name;
    for (const auto& attr : select->attributes) {
        if (attr.name == "name") select_name = attr.value;
    }
    EXPECT_EQ(select_name, "color");

    auto* opt = doc->find_element("option");
    ASSERT_NE(opt, nullptr);
    std::string opt_value;
    for (const auto& attr : opt->attributes) {
        if (attr.name == "value") opt_value = attr.value;
    }
    EXPECT_EQ(opt_value, "red");
    EXPECT_EQ(opt->text_content(), "Red");
}

// ---------------------------------------------------------------------------
// Cycle 482 — HTML parser: script async/defer, meta attributes, video/audio,
//             details/summary, table thead/tbody/tfoot, fieldset/legend,
//             pre/code, boolean attributes
// ---------------------------------------------------------------------------

TEST(TreeBuilder, ScriptWithAsyncDefer) {
    auto doc = parse("<html><head><script src=\"app.js\" async defer></script></head></html>");
    ASSERT_NE(doc, nullptr);

    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);

    std::string src_val;
    bool has_async = false, has_defer = false;
    for (const auto& attr : script->attributes) {
        if (attr.name == "src")   src_val   = attr.value;
        if (attr.name == "async") has_async = true;
        if (attr.name == "defer") has_defer = true;
    }
    EXPECT_EQ(src_val, "app.js");
    EXPECT_TRUE(has_async);
    EXPECT_TRUE(has_defer);
}

TEST(TreeBuilder, MetaTagAttributes) {
    auto doc = parse("<html><head>"
                     "<meta charset=\"utf-8\">"
                     "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                     "</head></html>");
    ASSERT_NE(doc, nullptr);

    auto metas = doc->find_all_elements("meta");
    ASSERT_GE(metas.size(), 1u);

    // First meta: charset
    std::string charset_val;
    for (const auto& attr : metas[0]->attributes) {
        if (attr.name == "charset") charset_val = attr.value;
    }
    EXPECT_EQ(charset_val, "utf-8");

    // Second meta: name + content
    if (metas.size() >= 2u) {
        std::string name_val, content_val;
        for (const auto& attr : metas[1]->attributes) {
            if (attr.name == "name")    name_val    = attr.value;
            if (attr.name == "content") content_val = attr.value;
        }
        EXPECT_EQ(name_val, "viewport");
        EXPECT_NE(content_val.find("device-width"), std::string::npos);
    }
}

TEST(TreeBuilder, VideoAudioElements) {
    auto doc = parse("<video controls><source src=\"movie.mp4\" type=\"video/mp4\"></video>"
                     "<audio controls><source src=\"song.ogg\" type=\"audio/ogg\"></audio>");
    ASSERT_NE(doc, nullptr);

    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    bool video_controls = false;
    for (const auto& attr : video->attributes) {
        if (attr.name == "controls") video_controls = true;
    }
    EXPECT_TRUE(video_controls);

    auto* source = doc->find_element("source");
    ASSERT_NE(source, nullptr);
    std::string src_val;
    for (const auto& attr : source->attributes) {
        if (attr.name == "src") src_val = attr.value;
    }
    EXPECT_EQ(src_val, "movie.mp4");

    auto* audio = doc->find_element("audio");
    ASSERT_NE(audio, nullptr);
}

TEST(TreeBuilder, DetailsAndSummary) {
    auto doc = parse("<details><summary>Click me</summary>Hidden content here.</details>");
    ASSERT_NE(doc, nullptr);

    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);

    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click me");

    // Full text of details includes both summary and hidden content
    std::string full_text = details->text_content();
    EXPECT_NE(full_text.find("Click me"), std::string::npos);
    EXPECT_NE(full_text.find("Hidden"), std::string::npos);
}

TEST(TreeBuilder, TableWithTheadTbodyTfoot) {
    auto doc = parse("<table>"
                     "<thead><tr><th>Name</th><th>Age</th></tr></thead>"
                     "<tbody><tr><td>Alice</td><td>30</td></tr></tbody>"
                     "<tfoot><tr><td colspan=\"2\">Footer</td></tr></tfoot>"
                     "</table>");
    ASSERT_NE(doc, nullptr);

    EXPECT_NE(doc->find_element("thead"), nullptr);
    EXPECT_NE(doc->find_element("tbody"), nullptr);
    EXPECT_NE(doc->find_element("tfoot"), nullptr);

    auto* th = doc->find_element("th");
    ASSERT_NE(th, nullptr);
    EXPECT_EQ(th->text_content(), "Name");

    // tbody's first td
    auto* tbody = doc->find_element("tbody");
    ASSERT_NE(tbody, nullptr);
    auto* td = tbody->find_element("td");
    ASSERT_NE(td, nullptr);
    EXPECT_EQ(td->text_content(), "Alice");
}

TEST(TreeBuilder, FieldsetWithLegend) {
    auto doc = parse("<fieldset><legend>Personal Info</legend>"
                     "<input type=\"text\" name=\"username\">"
                     "<input type=\"email\" name=\"email\">"
                     "</fieldset>");
    ASSERT_NE(doc, nullptr);

    auto* fieldset = doc->find_element("fieldset");
    ASSERT_NE(fieldset, nullptr);

    auto* legend = doc->find_element("legend");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->text_content(), "Personal Info");

    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    std::string type_val;
    for (const auto& attr : inputs[0]->attributes) {
        if (attr.name == "type") type_val = attr.value;
    }
    EXPECT_EQ(type_val, "text");
}

TEST(TreeBuilder, PreformattedContent) {
    auto doc = parse("<pre><code>  line1\n  line2\n</code></pre>");
    ASSERT_NE(doc, nullptr);

    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);

    auto* code = doc->find_element("code");
    ASSERT_NE(code, nullptr);

    std::string text = code->text_content();
    EXPECT_NE(text.find("line1"), std::string::npos);
    EXPECT_NE(text.find("line2"), std::string::npos);
}

TEST(TreeBuilder, BooleanAttributes) {
    auto doc = parse("<input type=\"checkbox\" checked disabled readonly>"
                     "<button type=\"submit\" disabled>Go</button>");
    ASSERT_NE(doc, nullptr);

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);

    bool has_checked = false, has_disabled = false, has_readonly = false;
    for (const auto& attr : input->attributes) {
        if (attr.name == "checked")  has_checked  = true;
        if (attr.name == "disabled") has_disabled = true;
        if (attr.name == "readonly") has_readonly = true;
    }
    EXPECT_TRUE(has_checked);
    EXPECT_TRUE(has_disabled);
    EXPECT_TRUE(has_readonly);

    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    bool btn_disabled = false;
    for (const auto& attr : button->attributes) {
        if (attr.name == "disabled") btn_disabled = true;
    }
    EXPECT_TRUE(btn_disabled);
    EXPECT_EQ(button->text_content(), "Go");
}

// ---------------------------------------------------------------------------
// Cycle 493 — HTML parser: figure/figcaption, iframe, definition lists,
//             single-quote attrs, nav+links, section+heading, class attr,
//             tokenizer single-quoted value
// ---------------------------------------------------------------------------

TEST(TreeBuilder, FigureAndFigcaption) {
    auto doc = parse("<figure><img src=\"photo.jpg\" alt=\"A photo\">"
                     "<figcaption>A scenic view</figcaption></figure>");
    ASSERT_NE(doc, nullptr);

    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    std::string src;
    for (const auto& attr : img->attributes) {
        if (attr.name == "src") src = attr.value;
    }
    EXPECT_EQ(src, "photo.jpg");

    auto* figcaption = doc->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->text_content(), "A scenic view");
}

TEST(TreeBuilder, IframeWithSrcAttribute) {
    auto doc = parse("<iframe src=\"https://example.com\" width=\"640\" height=\"480\"></iframe>");
    ASSERT_NE(doc, nullptr);

    auto* iframe = doc->find_element("iframe");
    ASSERT_NE(iframe, nullptr);

    std::string src, width;
    for (const auto& attr : iframe->attributes) {
        if (attr.name == "src")   src   = attr.value;
        if (attr.name == "width") width = attr.value;
    }
    EXPECT_EQ(src, "https://example.com");
    EXPECT_EQ(width, "640");
}

TEST(TreeBuilder, DefinitionList) {
    auto doc = parse("<dl><dt>Term1</dt><dd>Definition1</dd>"
                     "<dt>Term2</dt><dd>Definition2</dd></dl>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);

    auto dts = doc->find_all_elements("dt");
    auto dds = doc->find_all_elements("dd");
    ASSERT_EQ(dts.size(), 2u);
    ASSERT_EQ(dds.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term1");
    EXPECT_EQ(dds[0]->text_content(), "Definition1");
}

TEST(Tokenizer, SingleQuotedAttributeValue) {
    auto tokens = tokenize_all("<a href='https://example.com'>link</a>");
    std::string href;
    for (auto& t : tokens) {
        if (t.type == Token::StartTag && t.name == "a") {
            for (auto& attr : t.attributes) {
                if (attr.name == "href") href = attr.value;
            }
        }
    }
    EXPECT_EQ(href, "https://example.com");
}

TEST(TreeBuilder, NavWithLinks) {
    auto doc = parse("<nav><a href=\"/home\">Home</a><a href=\"/about\">About</a></nav>");
    ASSERT_NE(doc, nullptr);

    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);

    auto links = doc->find_all_elements("a");
    ASSERT_EQ(links.size(), 2u);
    EXPECT_EQ(links[0]->text_content(), "Home");
    EXPECT_EQ(links[1]->text_content(), "About");
}

TEST(TreeBuilder, SectionWithHeading) {
    auto doc = parse("<section><h2>Section Title</h2><p>Section body.</p></section>");
    ASSERT_NE(doc, nullptr);

    auto* section = doc->find_element("section");
    ASSERT_NE(section, nullptr);

    auto* h2 = doc->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->text_content(), "Section Title");

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Section body.");
}

TEST(TreeBuilder, MultipleClassesInAttribute) {
    auto doc = parse("<div class=\"container main hero\">content</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);

    std::string class_val;
    for (const auto& attr : div->attributes) {
        if (attr.name == "class") class_val = attr.value;
    }
    // The class attribute value should contain all three classes
    EXPECT_NE(class_val.find("container"), std::string::npos);
    EXPECT_NE(class_val.find("main"), std::string::npos);
    EXPECT_NE(class_val.find("hero"), std::string::npos);
}

TEST(TreeBuilder, DialogElement) {
    auto doc = parse("<dialog open><p>Are you sure?</p>"
                     "<button>OK</button><button>Cancel</button></dialog>");
    ASSERT_NE(doc, nullptr);

    auto* dialog = doc->find_element("dialog");
    ASSERT_NE(dialog, nullptr);

    bool has_open = false;
    for (const auto& attr : dialog->attributes) {
        if (attr.name == "open") has_open = true;
    }
    EXPECT_TRUE(has_open);

    auto buttons = doc->find_all_elements("button");
    ASSERT_EQ(buttons.size(), 2u);
    EXPECT_EQ(buttons[0]->text_content(), "OK");
    EXPECT_EQ(buttons[1]->text_content(), "Cancel");
}

// ============================================================================
// Cycle 506: HTML parser regression tests
// ============================================================================

TEST(TreeBuilder, ArticleElement) {
    auto doc = parse("<body><article><p>Content</p></article></body>");
    ASSERT_NE(doc, nullptr);
    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Content");
}

TEST(TreeBuilder, HeaderAndFooterElements) {
    auto doc = parse("<body><header>Top</header><footer>Bottom</footer></body>");
    ASSERT_NE(doc, nullptr);
    auto* header = doc->find_element("header");
    ASSERT_NE(header, nullptr);
    EXPECT_EQ(header->text_content(), "Top");
    auto* footer = doc->find_element("footer");
    ASSERT_NE(footer, nullptr);
    EXPECT_EQ(footer->text_content(), "Bottom");
}

TEST(TreeBuilder, H1ThroughH6AllParsed) {
    auto doc = parse("<body><h1>A</h1><h2>B</h2><h3>C</h3><h4>D</h4><h5>E</h5><h6>F</h6></body>");
    ASSERT_NE(doc, nullptr);
    EXPECT_NE(doc->find_element("h1"), nullptr);
    EXPECT_NE(doc->find_element("h2"), nullptr);
    EXPECT_NE(doc->find_element("h3"), nullptr);
    EXPECT_NE(doc->find_element("h4"), nullptr);
    EXPECT_NE(doc->find_element("h5"), nullptr);
    EXPECT_NE(doc->find_element("h6"), nullptr);
    EXPECT_EQ(doc->find_element("h1")->text_content(), "A");
    EXPECT_EQ(doc->find_element("h6")->text_content(), "F");
}

TEST(TreeBuilder, StyleElementInHead) {
    auto doc = parse("<head><style>body { color: red; }</style></head><body></body>");
    ASSERT_NE(doc, nullptr);
    auto* style = doc->find_element("style");
    ASSERT_NE(style, nullptr);
    EXPECT_NE(style->text_content().find("color"), std::string::npos);
}

TEST(TreeBuilder, SpanInsideParagraph) {
    auto doc = parse("<p>Hello <span>World</span></p>");
    ASSERT_NE(doc, nullptr);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "World");
}

TEST(TreeBuilder, StrongAndEmElements) {
    auto doc = parse("<p><strong>Bold</strong> and <em>Italic</em></p>");
    ASSERT_NE(doc, nullptr);
    EXPECT_NE(doc->find_element("strong"), nullptr);
    EXPECT_EQ(doc->find_element("strong")->text_content(), "Bold");
    EXPECT_NE(doc->find_element("em"), nullptr);
    EXPECT_EQ(doc->find_element("em")->text_content(), "Italic");
}

TEST(TreeBuilder, AnchorWithHrefAndTitle) {
    auto doc = parse(R"(<a href="https://example.com" title="Example">Click</a>)");
    ASSERT_NE(doc, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->text_content(), "Click");
    // Check attributes
    bool found_href = false, found_title = false;
    for (auto& attr : a->attributes) {
        if (attr.name == "href") found_href = true;
        if (attr.name == "title") found_title = true;
    }
    EXPECT_TRUE(found_href);
    EXPECT_TRUE(found_title);
}

TEST(TreeBuilder, AsideElement) {
    auto doc = parse("<body><main><p>Main</p></main><aside>Sidebar</aside></body>");
    ASSERT_NE(doc, nullptr);
    auto* aside = doc->find_element("aside");
    ASSERT_NE(aside, nullptr);
    EXPECT_EQ(aside->text_content(), "Sidebar");
}

// ============================================================================
// Cycle 514: HTML parser regression tests
// ============================================================================

TEST(TreeBuilder, MarkElement) {
    auto doc = parse("<body><p>Search <mark>result</mark> here</p></body>");
    ASSERT_NE(doc, nullptr);
    auto* mark = doc->find_element("mark");
    ASSERT_NE(mark, nullptr);
    EXPECT_EQ(mark->text_content(), "result");
}

TEST(TreeBuilder, SmallElement) {
    auto doc = parse("<body><p>Normal <small>fine print</small></p></body>");
    ASSERT_NE(doc, nullptr);
    auto* small = doc->find_element("small");
    ASSERT_NE(small, nullptr);
    EXPECT_EQ(small->text_content(), "fine print");
}

TEST(TreeBuilder, AbbrWithTitle) {
    auto doc = parse(R"(<body><abbr title="HyperText Markup Language">HTML</abbr></body>)");
    ASSERT_NE(doc, nullptr);
    auto* abbr = doc->find_element("abbr");
    ASSERT_NE(abbr, nullptr);
    EXPECT_EQ(abbr->text_content(), "HTML");
    bool has_title = false;
    for (auto& attr : abbr->attributes) {
        if (attr.name == "title") has_title = true;
    }
    EXPECT_TRUE(has_title);
}

TEST(TreeBuilder, BlockquoteElement) {
    auto doc = parse("<body><blockquote>A famous quote.</blockquote></body>");
    ASSERT_NE(doc, nullptr);
    auto* bq = doc->find_element("blockquote");
    ASSERT_NE(bq, nullptr);
    EXPECT_EQ(bq->text_content(), "A famous quote.");
}

TEST(TreeBuilder, CiteElement) {
    auto doc = parse("<body><p><cite>The Great Gatsby</cite></p></body>");
    ASSERT_NE(doc, nullptr);
    auto* cite = doc->find_element("cite");
    ASSERT_NE(cite, nullptr);
    EXPECT_EQ(cite->text_content(), "The Great Gatsby");
}

TEST(TreeBuilder, InlineCodeElement) {
    auto doc = parse("<body><p>Use <code>printf()</code> to print.</p></body>");
    ASSERT_NE(doc, nullptr);
    auto* code = doc->find_element("code");
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(code->text_content(), "printf()");
}

TEST(TreeBuilder, KbdElement) {
    auto doc = parse("<body><p>Press <kbd>Ctrl+C</kbd> to copy.</p></body>");
    ASSERT_NE(doc, nullptr);
    auto* kbd = doc->find_element("kbd");
    ASSERT_NE(kbd, nullptr);
    EXPECT_EQ(kbd->text_content(), "Ctrl+C");
}

TEST(TreeBuilder, SampElement) {
    auto doc = parse("<body><samp>Error: file not found</samp></body>");
    ASSERT_NE(doc, nullptr);
    auto* samp = doc->find_element("samp");
    ASSERT_NE(samp, nullptr);
    EXPECT_EQ(samp->text_content(), "Error: file not found");
}

// ============================================================================
// Cycle 522: HTML parser regression tests
// ============================================================================

TEST(TreeBuilder, SubElement) {
    auto doc = parse("<body><p>H<sub>2</sub>O</p></body>");
    ASSERT_NE(doc, nullptr);
    auto* sub = doc->find_element("sub");
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->text_content(), "2");
}

TEST(TreeBuilder, SupElement) {
    auto doc = parse("<body><p>x<sup>2</sup></p></body>");
    ASSERT_NE(doc, nullptr);
    auto* sup = doc->find_element("sup");
    ASSERT_NE(sup, nullptr);
    EXPECT_EQ(sup->text_content(), "2");
}

TEST(TreeBuilder, DelElement) {
    auto doc = parse("<body><del>old text</del></body>");
    ASSERT_NE(doc, nullptr);
    auto* del = doc->find_element("del");
    ASSERT_NE(del, nullptr);
    EXPECT_EQ(del->text_content(), "old text");
}

TEST(TreeBuilder, InsElement) {
    auto doc = parse("<body><ins>new text</ins></body>");
    ASSERT_NE(doc, nullptr);
    auto* ins = doc->find_element("ins");
    ASSERT_NE(ins, nullptr);
    EXPECT_EQ(ins->text_content(), "new text");
}

TEST(TreeBuilder, TimeElementWithDatetime) {
    auto doc = parse(R"(<body><time datetime="2024-01-15">January 15</time></body>)");
    ASSERT_NE(doc, nullptr);
    auto* time_el = doc->find_element("time");
    ASSERT_NE(time_el, nullptr);
    EXPECT_EQ(time_el->text_content(), "January 15");
}

TEST(TreeBuilder, OutputElement) {
    auto doc = parse("<body><output>42</output></body>");
    ASSERT_NE(doc, nullptr);
    auto* output = doc->find_element("output");
    ASSERT_NE(output, nullptr);
    EXPECT_EQ(output->text_content(), "42");
}

TEST(TreeBuilder, ProgressElement) {
    auto doc = parse(R"(<body><progress value="50" max="100"></progress></body>)");
    ASSERT_NE(doc, nullptr);
    auto* progress = doc->find_element("progress");
    ASSERT_NE(progress, nullptr);
    bool found_value = false;
    for (auto& attr : progress->attributes) {
        if (attr.name == "value") found_value = true;
    }
    EXPECT_TRUE(found_value);
}

TEST(TreeBuilder, MeterElement) {
    auto doc = parse(R"(<body><meter value="75" min="0" max="100">75%</meter></body>)");
    ASSERT_NE(doc, nullptr);
    auto* meter = doc->find_element("meter");
    ASSERT_NE(meter, nullptr);
    EXPECT_EQ(meter->text_content(), "75%");
}

// ============================================================================
// Cycle 529: HTML parser regression tests
// ============================================================================

TEST(TreeBuilder, WbrElement) {
    auto doc = parse("<body><p>long<wbr>word</p></body>");
    ASSERT_NE(doc, nullptr);
    auto* wbr = doc->find_element("wbr");
    ASSERT_NE(wbr, nullptr);
    EXPECT_EQ(wbr->tag_name, "wbr");
}

TEST(TreeBuilder, DialogElementSimpleContent) {
    auto doc = parse(R"(<body><dialog open>Hello</dialog></body>)");
    ASSERT_NE(doc, nullptr);
    auto* dialog = doc->find_element("dialog");
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->text_content(), "Hello");
}

TEST(TreeBuilder, SummaryInDetails) {
    auto doc = parse("<body><details><summary>Info</summary><p>Details here</p></details></body>");
    ASSERT_NE(doc, nullptr);
    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Info");
}

TEST(TreeBuilder, FigureWithFigcaption) {
    auto doc = parse("<body><figure><img alt=\"photo\"><figcaption>Caption</figcaption></figure></body>");
    ASSERT_NE(doc, nullptr);
    auto* figcaption = doc->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->text_content(), "Caption");
}

TEST(TreeBuilder, AddressElement) {
    auto doc = parse("<body><address>123 Main St</address></body>");
    ASSERT_NE(doc, nullptr);
    auto* address = doc->find_element("address");
    ASSERT_NE(address, nullptr);
    EXPECT_EQ(address->text_content(), "123 Main St");
}

TEST(TreeBuilder, MainElement) {
    auto doc = parse("<body><main><p>Main content</p></main></body>");
    ASSERT_NE(doc, nullptr);
    auto* main_el = doc->find_element("main");
    ASSERT_NE(main_el, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Main content");
}

TEST(TreeBuilder, SearchElement) {
    auto doc = parse("<body><search><input type=\"search\"></search></body>");
    ASSERT_NE(doc, nullptr);
    auto* search = doc->find_element("search");
    ASSERT_NE(search, nullptr);
    EXPECT_EQ(search->tag_name, "search");
}

TEST(TreeBuilder, HgroupElement) {
    auto doc = parse("<body><hgroup><h1>Title</h1><p>Subtitle</p></hgroup></body>");
    ASSERT_NE(doc, nullptr);
    auto* hgroup = doc->find_element("hgroup");
    ASSERT_NE(hgroup, nullptr);
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Title");
}

// ============================================================================
// Cycle 539: HTML parser regression tests
// ============================================================================

TEST(TreeBuilder, FormElement) {
    auto doc = parse(R"(<body><form action="/submit" method="POST"><input type="text"></form></body>)");
    ASSERT_NE(doc, nullptr);
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    bool has_action = false;
    for (auto& attr : form->attributes) {
        if (attr.name == "action") has_action = true;
    }
    EXPECT_TRUE(has_action);
}

TEST(TreeBuilder, TableElement) {
    auto doc = parse("<body><table><tr><td>Cell</td></tr></table></body>");
    ASSERT_NE(doc, nullptr);
    auto* td = doc->find_element("td");
    ASSERT_NE(td, nullptr);
    EXPECT_EQ(td->text_content(), "Cell");
}

TEST(TreeBuilder, FieldsetLegendText) {
    auto doc = parse("<body><fieldset><legend>Group</legend></fieldset></body>");
    ASSERT_NE(doc, nullptr);
    auto* legend = doc->find_element("legend");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->text_content(), "Group");
}

TEST(TreeBuilder, SelectOptionsContent) {
    auto doc = parse(R"(<body><select><option value="1">One</option><option value="2">Two</option></select></body>)");
    ASSERT_NE(doc, nullptr);
    auto* select = doc->find_element("select");
    ASSERT_NE(select, nullptr);
    auto* option = doc->find_element("option");
    ASSERT_NE(option, nullptr);
    EXPECT_EQ(option->text_content(), "One");
}

TEST(TreeBuilder, IframeElement) {
    auto doc = parse(R"(<body><iframe src="https://example.com" title="embed"></iframe></body>)");
    ASSERT_NE(doc, nullptr);
    auto* iframe = doc->find_element("iframe");
    ASSERT_NE(iframe, nullptr);
    bool has_src = false;
    for (auto& attr : iframe->attributes) {
        if (attr.name == "src") has_src = true;
    }
    EXPECT_TRUE(has_src);
}

TEST(TreeBuilder, TwoSectionsInMain) {
    auto doc = parse("<body><main><section id=\"s1\">A</section><section id=\"s2\">B</section></main></body>");
    ASSERT_NE(doc, nullptr);
    auto* main_el = doc->find_element("main");
    ASSERT_NE(main_el, nullptr);
    auto* section = doc->find_element("section");
    ASSERT_NE(section, nullptr);
    EXPECT_EQ(section->text_content(), "A");
}

TEST(TreeBuilder, PreformattedText) {
    auto doc = parse("<body><pre>  indented\n  text  </pre></body>");
    ASSERT_NE(doc, nullptr);
    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
    EXPECT_NE(pre->text_content().find("indented"), std::string::npos);
}

TEST(TreeBuilder, OlWithListItems) {
    auto doc = parse("<body><ol><li>First</li><li>Second</li></ol></body>");
    ASSERT_NE(doc, nullptr);
    auto* li = doc->find_element("li");
    ASSERT_NE(li, nullptr);
    EXPECT_EQ(li->text_content(), "First");
}

// ============================================================================
// Cycle 550: HTML parser regression tests (milestone!)
// ============================================================================

TEST(TreeBuilder, HeadElementContainsTitle) {
    auto doc = parse("<html><head><title>Page Title</title></head><body></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* title = doc->find_element("title");
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->text_content(), "Page Title");
}

TEST(TreeBuilder, MetaCharsetInHead) {
    auto doc = parse("<html><head><meta charset=\"UTF-8\"></head><body></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* meta = doc->find_element("meta");
    ASSERT_NE(meta, nullptr);
    bool has_charset = false;
    for (auto& attr : meta->attributes) {
        if (attr.name == "charset") has_charset = true;
    }
    EXPECT_TRUE(has_charset);
}

TEST(TreeBuilder, ScriptTagParsed) {
    auto doc = parse("<html><head><script>var x = 1;</script></head><body></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
}

TEST(TreeBuilder, StyleTagParsed) {
    auto doc = parse("<html><head><style>body { color: red; }</style></head><body></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* style = doc->find_element("style");
    ASSERT_NE(style, nullptr);
}

TEST(TreeBuilder, LinkTagParsed) {
    auto doc = parse(R"(<html><head><link rel="stylesheet" href="style.css"></head><body></body></html>)");
    ASSERT_NE(doc, nullptr);
    auto* link = doc->find_element("link");
    ASSERT_NE(link, nullptr);
    bool has_href = false;
    for (auto& attr : link->attributes) {
        if (attr.name == "href") has_href = true;
    }
    EXPECT_TRUE(has_href);
}

TEST(TreeBuilder, H2InSection) {
    auto doc = parse("<body><section><h2>Section Title</h2><p>Content</p></section></body>");
    ASSERT_NE(doc, nullptr);
    auto* h2 = doc->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->text_content(), "Section Title");
}

TEST(TreeBuilder, EmbedElement) {
    auto doc = parse(R"(<body><embed src="video.mp4" type="video/mp4"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* embed = doc->find_element("embed");
    ASSERT_NE(embed, nullptr);
    bool has_src = false;
    for (auto& attr : embed->attributes) {
        if (attr.name == "src") has_src = true;
    }
    EXPECT_TRUE(has_src);
}

TEST(TreeBuilder, ObjectWithParam) {
    auto doc = parse("<body><object data=\"file.swf\"><param name=\"autoplay\" value=\"true\"></object></body>");
    ASSERT_NE(doc, nullptr);
    auto* obj = doc->find_element("object");
    ASSERT_NE(obj, nullptr);
    auto* param = doc->find_element("param");
    ASSERT_NE(param, nullptr);
}

// ============================================================================
// Cycle 558: HTML parser regression tests
// ============================================================================

TEST(TreeBuilder, AudioElement) {
    auto doc = parse(R"(<body><audio src="sound.mp3" controls></audio></body>)");
    ASSERT_NE(doc, nullptr);
    auto* audio = doc->find_element("audio");
    ASSERT_NE(audio, nullptr);
    bool has_src = false;
    for (auto& attr : audio->attributes) {
        if (attr.name == "src") has_src = true;
    }
    EXPECT_TRUE(has_src);
}

TEST(TreeBuilder, VideoElement) {
    auto doc = parse(R"(<body><video src="clip.mp4" width="640" height="480"></video></body>)");
    ASSERT_NE(doc, nullptr);
    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    bool has_width = false;
    for (auto& attr : video->attributes) {
        if (attr.name == "width") has_width = true;
    }
    EXPECT_TRUE(has_width);
}

TEST(TreeBuilder, CanvasElement) {
    auto doc = parse(R"(<body><canvas id="myCanvas" width="300" height="150"></canvas></body>)");
    ASSERT_NE(doc, nullptr);
    auto* canvas = doc->find_element("canvas");
    ASSERT_NE(canvas, nullptr);
    bool has_id = false;
    for (auto& attr : canvas->attributes) {
        if (attr.name == "id") has_id = true;
    }
    EXPECT_TRUE(has_id);
}

TEST(TreeBuilder, InputTypeEmail) {
    auto doc = parse(R"(<body><input type="email" name="email" required></body>)");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool has_required = false;
    for (auto& attr : input->attributes) {
        if (attr.name == "required") has_required = true;
    }
    EXPECT_TRUE(has_required);
}

TEST(TreeBuilder, TextareaElement) {
    auto doc = parse(R"(<body><textarea rows="5" cols="40">Default text</textarea></body>)");
    ASSERT_NE(doc, nullptr);
    auto* ta = doc->find_element("textarea");
    ASSERT_NE(ta, nullptr);
    EXPECT_EQ(ta->text_content(), "Default text");
}

TEST(TreeBuilder, ButtonTypeSubmit) {
    auto doc = parse(R"(<body><button type="submit">Submit</button></body>)");
    ASSERT_NE(doc, nullptr);
    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text_content(), "Submit");
}

TEST(TreeBuilder, LabelWithForAttr) {
    auto doc = parse(R"(<body><label for="email">Email:</label><input id="email" type="email"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* label = doc->find_element("label");
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->text_content(), "Email:");
}

TEST(TreeBuilder, DatalistElement) {
    auto doc = parse("<body><datalist id=\"colors\"><option value=\"red\"><option value=\"blue\"></datalist></body>");
    ASSERT_NE(doc, nullptr);
    auto* datalist = doc->find_element("datalist");
    ASSERT_NE(datalist, nullptr);
    auto* option = doc->find_element("option");
    ASSERT_NE(option, nullptr);
}

// ============================================================================
// Cycle 564: More HTML parser tree builder tests
// ============================================================================

TEST(TreeBuilder, NavElement) {
    auto doc = parse(R"(<body><nav><a href="/home">Home</a></nav></body>)");
    ASSERT_NE(doc, nullptr);
    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->text_content(), "Home");
}

TEST(TreeBuilder, AsideWithParagraph) {
    auto doc = parse(R"(<body><aside><p>Sidebar content</p></aside></body>)");
    ASSERT_NE(doc, nullptr);
    auto* aside = doc->find_element("aside");
    ASSERT_NE(aside, nullptr);
}

TEST(TreeBuilder, FooterElement) {
    auto doc = parse(R"(<body><footer><p>Copyright 2025</p></footer></body>)");
    ASSERT_NE(doc, nullptr);
    auto* footer = doc->find_element("footer");
    ASSERT_NE(footer, nullptr);
    EXPECT_EQ(footer->find_element("p")->text_content(), "Copyright 2025");
}

TEST(TreeBuilder, HeaderElement) {
    auto doc = parse(R"(<body><header><h1>Site Title</h1></header></body>)");
    ASSERT_NE(doc, nullptr);
    auto* header = doc->find_element("header");
    ASSERT_NE(header, nullptr);
    ASSERT_NE(header->find_element("h1"), nullptr);
}

TEST(TreeBuilder, BlockquoteContainsParagraph) {
    auto doc = parse(R"(<body><blockquote><p>Quote text</p></blockquote></body>)");
    ASSERT_NE(doc, nullptr);
    auto* bq = doc->find_element("blockquote");
    ASSERT_NE(bq, nullptr);
}

TEST(TreeBuilder, DivWithIdAndClass) {
    auto doc = parse(R"(<body><div id="main" class="container">content</div></body>)");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    bool has_id = false, has_class = false;
    for (auto& attr : div->attributes) {
        if (attr.name == "id" && attr.value == "main") has_id = true;
        if (attr.name == "class" && attr.value == "container") has_class = true;
    }
    EXPECT_TRUE(has_id);
    EXPECT_TRUE(has_class);
}

TEST(TreeBuilder, SpanTextContentIsWorld) {
    auto doc = parse(R"(<body><p>Hello <span>world</span>!</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "world");
}

TEST(TreeBuilder, UlWithThreeItems) {
    auto doc = parse(R"(<body><ul><li>A</li><li>B</li><li>C</li></ul></body>)");
    ASSERT_NE(doc, nullptr);
    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    int li_count = 0;
    for (auto& child : ul->children) {
        if (child->tag_name == "li") li_count++;
    }
    EXPECT_EQ(li_count, 3);
}

// ============================================================================
// Cycle 575: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, StrongElement) {
    auto doc = parse(R"(<body><p>This is <strong>important</strong> text</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "important");
}

TEST(TreeBuilder, EmElement) {
    auto doc = parse(R"(<body><p>This is <em>emphasized</em> text</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "emphasized");
}

TEST(TreeBuilder, AnchorWithHref) {
    auto doc = parse(R"(<body><a href="https://example.com">Click here</a></body>)");
    ASSERT_NE(doc, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->text_content(), "Click here");
    bool found = false;
    for (auto& attr : a->attributes) {
        if (attr.name == "href") found = true;
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ImgWithSrcAndAlt) {
    auto doc = parse(R"(<body><img src="photo.jpg" alt="A photo"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    bool has_src = false, has_alt = false;
    for (auto& attr : img->attributes) {
        if (attr.name == "src" && attr.value == "photo.jpg") has_src = true;
        if (attr.name == "alt") has_alt = true;
    }
    EXPECT_TRUE(has_src);
    EXPECT_TRUE(has_alt);
}

TEST(TreeBuilder, InputWithPlaceholder) {
    auto doc = parse(R"(<body><input type="text" placeholder="Enter name"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found_ph = false;
    for (auto& attr : input->attributes) {
        if (attr.name == "placeholder") found_ph = true;
    }
    EXPECT_TRUE(found_ph);
}

TEST(TreeBuilder, H3InArticle) {
    auto doc = parse(R"(<body><article><h3>Subheading</h3><p>Content</p></article></body>)");
    ASSERT_NE(doc, nullptr);
    auto* h3 = doc->find_element("h3");
    ASSERT_NE(h3, nullptr);
    EXPECT_EQ(h3->text_content(), "Subheading");
}

TEST(TreeBuilder, CodeInsidePre) {
    auto doc = parse(R"(<body><pre><code>int main() {}</code></pre></body>)");
    ASSERT_NE(doc, nullptr);
    auto* code = doc->find_element("code");
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(code->text_content(), "int main() {}");
}

TEST(TreeBuilder, SmallElementPrice) {
    auto doc = parse(R"(<body><p>Price: <small>$9.99</small></p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* small = doc->find_element("small");
    ASSERT_NE(small, nullptr);
    EXPECT_EQ(small->text_content(), "$9.99");
}

// ============================================================================
// Cycle 584: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, AbbrElement) {
    auto doc = parse(R"(<body><abbr title="World Wide Web">WWW</abbr></body>)");
    ASSERT_NE(doc, nullptr);
    auto* abbr = doc->find_element("abbr");
    ASSERT_NE(abbr, nullptr);
    EXPECT_EQ(abbr->text_content(), "WWW");
}

TEST(TreeBuilder, MarkWithHighlightedText) {
    auto doc = parse(R"(<body><p>Search <mark>result</mark> here</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* mark = doc->find_element("mark");
    ASSERT_NE(mark, nullptr);
    EXPECT_EQ(mark->text_content(), "result");
}

TEST(TreeBuilder, TimeElementDatetime) {
    auto doc = parse(R"(<body><time datetime="2025-01-01">New Year</time></body>)");
    ASSERT_NE(doc, nullptr);
    auto* time = doc->find_element("time");
    ASSERT_NE(time, nullptr);
    EXPECT_EQ(time->text_content(), "New Year");
    bool has_dt = false;
    for (auto& attr : time->attributes) {
        if (attr.name == "datetime") has_dt = true;
    }
    EXPECT_TRUE(has_dt);
}

TEST(TreeBuilder, ProgressWithMaxAttr) {
    auto doc = parse(R"(<body><progress value="70" max="100"></progress></body>)");
    ASSERT_NE(doc, nullptr);
    auto* progress = doc->find_element("progress");
    ASSERT_NE(progress, nullptr);
    bool has_max = false;
    for (auto& attr : progress->attributes) {
        if (attr.name == "max" && attr.value == "100") has_max = true;
    }
    EXPECT_TRUE(has_max);
}

TEST(TreeBuilder, MeterWithTextContent) {
    auto doc = parse(R"(<body><meter value="0.6">60%</meter></body>)");
    ASSERT_NE(doc, nullptr);
    auto* meter = doc->find_element("meter");
    ASSERT_NE(meter, nullptr);
    EXPECT_EQ(meter->text_content(), "60%");
}

TEST(TreeBuilder, OutputWithForAttr) {
    auto doc = parse(R"(<body><output for="a b">Result</output></body>)");
    ASSERT_NE(doc, nullptr);
    auto* output = doc->find_element("output");
    ASSERT_NE(output, nullptr);
    bool has_for = false;
    for (auto& attr : output->attributes) {
        if (attr.name == "for") has_for = true;
    }
    EXPECT_TRUE(has_for);
}

TEST(TreeBuilder, KbdShortcutText) {
    auto doc = parse(R"(<body><p>Press <kbd>Ctrl+C</kbd></p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* kbd = doc->find_element("kbd");
    ASSERT_NE(kbd, nullptr);
    EXPECT_EQ(kbd->text_content(), "Ctrl+C");
}

TEST(TreeBuilder, SampOutputText) {
    auto doc = parse(R"(<body><p>Output: <samp>Hello, World!</samp></p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* samp = doc->find_element("samp");
    ASSERT_NE(samp, nullptr);
    EXPECT_EQ(samp->text_content(), "Hello, World!");
}

// ============================================================================
// Cycle 592: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, CitationElement) {
    auto doc = parse(R"(<body><blockquote><p>Text</p><cite>Source</cite></blockquote></body>)");
    ASSERT_NE(doc, nullptr);
    auto* cite = doc->find_element("cite");
    ASSERT_NE(cite, nullptr);
    EXPECT_EQ(cite->text_content(), "Source");
}

TEST(TreeBuilder, BdiElement) {
    auto doc = parse(R"(<body><p><bdi>مرحبا</bdi></p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* bdi = doc->find_element("bdi");
    ASSERT_NE(bdi, nullptr);
}

TEST(TreeBuilder, RubyWithRtAnnotation) {
    auto doc = parse(R"(<body><ruby>漢<rt>かん</rt></ruby></body>)");
    ASSERT_NE(doc, nullptr);
    auto* ruby = doc->find_element("ruby");
    ASSERT_NE(ruby, nullptr);
    auto* rt = doc->find_element("rt");
    ASSERT_NE(rt, nullptr);
}

TEST(TreeBuilder, SubScriptH2O) {
    auto doc = parse(R"(<body><p>H<sub>2</sub>O</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* sub = doc->find_element("sub");
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->text_content(), "2");
    // Verify parent is p
    EXPECT_NE(doc->find_element("p"), nullptr);
}

TEST(TreeBuilder, SupScriptMC2) {
    auto doc = parse(R"(<body><p>E = mc<sup>2</sup></p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* sup = doc->find_element("sup");
    ASSERT_NE(sup, nullptr);
    EXPECT_EQ(sup->text_content(), "2");
}

TEST(TreeBuilder, InsertedTextContent) {
    auto doc = parse(R"(<body><p>This was <ins>inserted</ins> text</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* ins = doc->find_element("ins");
    ASSERT_NE(ins, nullptr);
    EXPECT_EQ(ins->text_content(), "inserted");
}

TEST(TreeBuilder, DeletedTextContent) {
    auto doc = parse(R"(<body><p>This was <del>deleted</del> text</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* del = doc->find_element("del");
    ASSERT_NE(del, nullptr);
    EXPECT_EQ(del->text_content(), "deleted");
}

TEST(TreeBuilder, VarElement) {
    auto doc = parse(R"(<body><p>The variable <var>x</var> is defined</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* var = doc->find_element("var");
    ASSERT_NE(var, nullptr);
    EXPECT_EQ(var->text_content(), "x");
}

// ============================================================================
// Cycle 598: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, WbrInsideParagraph) {
    auto doc = parse(R"(<body><p>word<wbr>break</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* wbr = doc->find_element("wbr");
    ASSERT_NE(wbr, nullptr);
}

TEST(TreeBuilder, BrElement) {
    auto doc = parse(R"(<body><p>line one<br>line two</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* br = doc->find_element("br");
    ASSERT_NE(br, nullptr);
}

TEST(TreeBuilder, HrElement) {
    auto doc = parse(R"(<body><p>Above</p><hr><p>Below</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
}

TEST(TreeBuilder, TableWithCaption) {
    auto doc = parse(R"(<body><table><caption>My Table</caption><tr><td>A</td></tr></table></body>)");
    ASSERT_NE(doc, nullptr);
    auto* caption = doc->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->text_content(), "My Table");
}

TEST(TreeBuilder, TableRowElement) {
    auto doc = parse(R"(<body><table><tr><td>cell</td></tr></table></body>)");
    ASSERT_NE(doc, nullptr);
    auto* tr = doc->find_element("tr");
    ASSERT_NE(tr, nullptr);
}

TEST(TreeBuilder, TableDataElement) {
    auto doc = parse(R"(<body><table><tr><td>data</td></tr></table></body>)");
    ASSERT_NE(doc, nullptr);
    auto* td = doc->find_element("td");
    ASSERT_NE(td, nullptr);
    EXPECT_EQ(td->text_content(), "data");
}

TEST(TreeBuilder, TableHeaderElement) {
    auto doc = parse(R"(<body><table><tr><th>Header</th></tr></table></body>)");
    ASSERT_NE(doc, nullptr);
    auto* th = doc->find_element("th");
    ASSERT_NE(th, nullptr);
    EXPECT_EQ(th->text_content(), "Header");
}

TEST(TreeBuilder, FieldsetLegendIsSettings) {
    auto doc = parse(R"(<body><fieldset><legend>Settings</legend><input type="checkbox"></fieldset></body>)");
    ASSERT_NE(doc, nullptr);
    auto* legend = doc->find_element("legend");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->text_content(), "Settings");
}

// ============================================================================
// Cycle 609: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, SelectWithOption) {
    auto doc = parse(R"(<body><select><option value="1">One</option><option value="2">Two</option></select></body>)");
    ASSERT_NE(doc, nullptr);
    auto* select = doc->find_element("select");
    ASSERT_NE(select, nullptr);
}

TEST(TreeBuilder, OptionTextContent) {
    auto doc = parse(R"(<body><select><option value="a">Alpha</option></select></body>)");
    ASSERT_NE(doc, nullptr);
    auto* option = doc->find_element("option");
    ASSERT_NE(option, nullptr);
    EXPECT_EQ(option->text_content(), "Alpha");
}

TEST(TreeBuilder, TextareaWithRowsCols) {
    auto doc = parse(R"(<body><textarea rows="4" cols="50">Enter text here</textarea></body>)");
    ASSERT_NE(doc, nullptr);
    auto* ta = doc->find_element("textarea");
    ASSERT_NE(ta, nullptr);
}

TEST(TreeBuilder, ButtonWithText) {
    auto doc = parse(R"(<body><button type="submit">Submit</button></body>)");
    ASSERT_NE(doc, nullptr);
    auto* btn = doc->find_element("button");
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->text_content(), "Submit");
}

TEST(TreeBuilder, LabelForInput) {
    auto doc = parse(R"(<body><label for="name">Name:</label><input id="name" type="text"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* label = doc->find_element("label");
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->text_content(), "Name:");
}

TEST(TreeBuilder, FormWithActionMethod) {
    auto doc = parse(R"(<body><form action="/submit" method="post"><input type="text"></form></body>)");
    ASSERT_NE(doc, nullptr);
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
}

TEST(TreeBuilder, IframeSrcTitle) {
    auto doc = parse(R"(<body><iframe src="https://example.com" title="Example"></iframe></body>)");
    ASSERT_NE(doc, nullptr);
    auto* iframe = doc->find_element("iframe");
    ASSERT_NE(iframe, nullptr);
}

TEST(TreeBuilder, CanvasWidthHeight) {
    auto doc = parse(R"(<body><canvas id="myCanvas" width="300" height="150"></canvas></body>)");
    ASSERT_NE(doc, nullptr);
    auto* canvas = doc->find_element("canvas");
    ASSERT_NE(canvas, nullptr);
}

// ============================================================================
// Cycle 618: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, VideoWithControls) {
    auto doc = parse(R"(<body><video src="movie.mp4" controls></video></body>)");
    ASSERT_NE(doc, nullptr);
    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
}

TEST(TreeBuilder, AudioWithControls) {
    auto doc = parse(R"(<body><audio src="sound.mp3" controls></audio></body>)");
    ASSERT_NE(doc, nullptr);
    auto* audio = doc->find_element("audio");
    ASSERT_NE(audio, nullptr);
}

TEST(TreeBuilder, PictureWithSource) {
    auto doc = parse(R"(<body><picture><source srcset="img.webp" type="image/webp"><img src="img.jpg" alt="img"></picture></body>)");
    ASSERT_NE(doc, nullptr);
    auto* picture = doc->find_element("picture");
    ASSERT_NE(picture, nullptr);
}

TEST(TreeBuilder, FigcaptionTextIsPhoto) {
    auto doc = parse(R"(<body><figure><img src="photo.jpg"><figcaption>A photo</figcaption></figure></body>)");
    ASSERT_NE(doc, nullptr);
    auto* figcaption = doc->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->text_content(), "A photo");
}

TEST(TreeBuilder, DetailsSummaryClickMe) {
    auto doc = parse(R"(<body><details><summary>Click me</summary><p>Details here</p></details></body>)");
    ASSERT_NE(doc, nullptr);
    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click me");
}

TEST(TreeBuilder, DialogOpenAttribute) {
    auto doc = parse(R"(<body><dialog open><p>Dialog content</p></dialog></body>)");
    ASSERT_NE(doc, nullptr);
    auto* dialog = doc->find_element("dialog");
    ASSERT_NE(dialog, nullptr);
}

TEST(TreeBuilder, AddressWithLink) {
    auto doc = parse(R"(<body><address>Contact: <a href="mailto:info@example.com">info@example.com</a></address></body>)");
    ASSERT_NE(doc, nullptr);
    auto* address = doc->find_element("address");
    ASSERT_NE(address, nullptr);
}

TEST(TreeBuilder, MainWithH1) {
    auto doc = parse(R"(<body><main><h1>Main content</h1></main></body>)");
    ASSERT_NE(doc, nullptr);
    auto* main_el = doc->find_element("main");
    ASSERT_NE(main_el, nullptr);
}

// ============================================================================
// Cycle 627: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, ScriptElement) {
    auto doc = parse(R"(<head><script src="app.js"></script></head><body></body>)");
    ASSERT_NE(doc, nullptr);
    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
}

TEST(TreeBuilder, StyleElement) {
    auto doc = parse(R"(<head><style>body { margin: 0; }</style></head><body></body>)");
    ASSERT_NE(doc, nullptr);
    auto* style = doc->find_element("style");
    ASSERT_NE(style, nullptr);
}

TEST(TreeBuilder, LinkElement) {
    auto doc = parse(R"(<head><link rel="stylesheet" href="styles.css"></head><body></body>)");
    ASSERT_NE(doc, nullptr);
    auto* link = doc->find_element("link");
    ASSERT_NE(link, nullptr);
}

TEST(TreeBuilder, MetaElement) {
    auto doc = parse(R"(<head><meta charset="UTF-8"></head><body></body>)");
    ASSERT_NE(doc, nullptr);
    auto* meta = doc->find_element("meta");
    ASSERT_NE(meta, nullptr);
}

TEST(TreeBuilder, TitleElement) {
    auto doc = parse(R"(<head><title>My Page</title></head><body></body>)");
    ASSERT_NE(doc, nullptr);
    auto* title = doc->find_element("title");
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->text_content(), "My Page");
}

TEST(TreeBuilder, BaseElement) {
    auto doc = parse(R"(<head><base href="https://example.com/"></head><body></body>)");
    ASSERT_NE(doc, nullptr);
    auto* base = doc->find_element("base");
    ASSERT_NE(base, nullptr);
}

TEST(TreeBuilder, NoscriptElement) {
    auto doc = parse(R"(<body><noscript><p>JavaScript required</p></noscript></body>)");
    ASSERT_NE(doc, nullptr);
    auto* noscript = doc->find_element("noscript");
    ASSERT_NE(noscript, nullptr);
}

TEST(TreeBuilder, TemplateElement) {
    auto doc = parse(R"(<body><template id="tmpl"><p>Template content</p></template></body>)");
    ASSERT_NE(doc, nullptr);
    auto* tmpl = doc->find_element("template");
    ASSERT_NE(tmpl, nullptr);
}

// ============================================================================
// Cycle 635: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, HeadElementExists) {
    auto doc = parse(R"(<html><head></head><body></body></html>)");
    ASSERT_NE(doc, nullptr);
    auto* head = doc->find_element("head");
    ASSERT_NE(head, nullptr);
}

TEST(TreeBuilder, BodyElementExists) {
    auto doc = parse(R"(<html><head></head><body></body></html>)");
    ASSERT_NE(doc, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
}

TEST(TreeBuilder, SpanInsideDiv) {
    auto doc = parse(R"(<body><div><span>text</span></div></body>)");
    ASSERT_NE(doc, nullptr);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
}

TEST(TreeBuilder, UlWithThreeLi) {
    auto doc = parse(R"(<body><ul><li>a</li><li>b</li><li>c</li></ul></body>)");
    ASSERT_NE(doc, nullptr);
    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    EXPECT_GE(ul->children.size(), 3u);
}

TEST(TreeBuilder, OrderedListOl) {
    auto doc = parse(R"(<body><ol><li>first</li><li>second</li></ol></body>)");
    ASSERT_NE(doc, nullptr);
    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
}

TEST(TreeBuilder, DefinitionListDl) {
    auto doc = parse(R"(<body><dl><dt>term</dt><dd>definition</dd></dl></body>)");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
}

TEST(TreeBuilder, EmphasisElement) {
    auto doc = parse(R"(<body><p><em>emphasized</em></p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "emphasized");
}

TEST(TreeBuilder, StrongElementWithBoldText) {
    auto doc = parse(R"(<body><p><strong>bold text</strong></p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "bold text");
}

// ============================================================================
// Cycle 643: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, CodeWithJsContent) {
    auto doc = parse(R"(<body><code>var x = 1;</code></body>)");
    ASSERT_NE(doc, nullptr);
    auto* code = doc->find_element("code");
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(code->text_content(), "var x = 1;");
}

TEST(TreeBuilder, PreformattedIndentedText) {
    auto doc = parse(R"(<body><pre>  indented</pre></body>)");
    ASSERT_NE(doc, nullptr);
    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
}

TEST(TreeBuilder, BlockquoteWithParagraph) {
    auto doc = parse(R"(<body><blockquote><p>quote</p></blockquote></body>)");
    ASSERT_NE(doc, nullptr);
    auto* bq = doc->find_element("blockquote");
    ASSERT_NE(bq, nullptr);
}

TEST(TreeBuilder, AbbrWithTitleAttribute) {
    auto doc = parse(R"(<body><abbr title="HyperText Markup Language">HTML</abbr></body>)");
    ASSERT_NE(doc, nullptr);
    auto* abbr = doc->find_element("abbr");
    ASSERT_NE(abbr, nullptr);
    EXPECT_EQ(abbr->text_content(), "HTML");
}

TEST(TreeBuilder, SmallWithFinePrint) {
    auto doc = parse(R"(<body><small>fine print</small></body>)");
    ASSERT_NE(doc, nullptr);
    auto* small = doc->find_element("small");
    ASSERT_NE(small, nullptr);
}

TEST(TreeBuilder, MarkHighlightedText) {
    auto doc = parse(R"(<body><mark>highlighted</mark></body>)");
    ASSERT_NE(doc, nullptr);
    auto* mark = doc->find_element("mark");
    ASSERT_NE(mark, nullptr);
    EXPECT_EQ(mark->text_content(), "highlighted");
}

TEST(TreeBuilder, TimeNewYearDatetime) {
    auto doc = parse(R"(<body><time datetime="2024-01-01">New Year</time></body>)");
    ASSERT_NE(doc, nullptr);
    auto* time = doc->find_element("time");
    ASSERT_NE(time, nullptr);
}

TEST(TreeBuilder, KbdCtrlC) {
    auto doc = parse(R"(<body><kbd>Ctrl+C</kbd></body>)");
    ASSERT_NE(doc, nullptr);
    auto* kbd = doc->find_element("kbd");
    ASSERT_NE(kbd, nullptr);
    EXPECT_EQ(kbd->text_content(), "Ctrl+C");
}

// ============================================================================
// Cycle 652: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, SampElementOutputText) {
    auto doc = parse(R"(<body><samp>output text</samp></body>)");
    ASSERT_NE(doc, nullptr);
    auto* samp = doc->find_element("samp");
    ASSERT_NE(samp, nullptr);
    EXPECT_EQ(samp->text_content(), "output text");
}

TEST(TreeBuilder, VarElementVarX) {
    auto doc = parse(R"(<body><var>x</var></body>)");
    ASSERT_NE(doc, nullptr);
    auto* var = doc->find_element("var");
    ASSERT_NE(var, nullptr);
    EXPECT_EQ(var->text_content(), "x");
}

TEST(TreeBuilder, CiteBookTitle) {
    auto doc = parse(R"(<body><cite>Some Book</cite></body>)");
    ASSERT_NE(doc, nullptr);
    auto* cite = doc->find_element("cite");
    ASSERT_NE(cite, nullptr);
    EXPECT_EQ(cite->text_content(), "Some Book");
}

TEST(TreeBuilder, QInlineQuote) {
    auto doc = parse(R"(<body><q>inline quote</q></body>)");
    ASSERT_NE(doc, nullptr);
    auto* q = doc->find_element("q");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->text_content(), "inline quote");
}

TEST(TreeBuilder, ItalicElement) {
    auto doc = parse(R"(<body><i>italic text</i></body>)");
    ASSERT_NE(doc, nullptr);
    auto* i = doc->find_element("i");
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->text_content(), "italic text");
}

TEST(TreeBuilder, BoldElement) {
    auto doc = parse(R"(<body><b>bold text</b></body>)");
    ASSERT_NE(doc, nullptr);
    auto* b = doc->find_element("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->text_content(), "bold text");
}

TEST(TreeBuilder, UnderlineElement) {
    auto doc = parse(R"(<body><u>underlined</u></body>)");
    ASSERT_NE(doc, nullptr);
    auto* u = doc->find_element("u");
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(u->text_content(), "underlined");
}

TEST(TreeBuilder, StrikethroughElement) {
    auto doc = parse(R"(<body><s>strikethrough</s></body>)");
    ASSERT_NE(doc, nullptr);
    auto* s = doc->find_element("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->text_content(), "strikethrough");
}

// ============================================================================
// Cycle 663: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, SubScriptElement) {
    auto doc = parse(R"(<body><sub>2</sub></body>)");
    ASSERT_NE(doc, nullptr);
    auto* sub = doc->find_element("sub");
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->text_content(), "2");
}

TEST(TreeBuilder, SuperScriptElement) {
    auto doc = parse(R"(<body><sup>3</sup></body>)");
    ASSERT_NE(doc, nullptr);
    auto* sup = doc->find_element("sup");
    ASSERT_NE(sup, nullptr);
    EXPECT_EQ(sup->text_content(), "3");
}

TEST(TreeBuilder, SpanWithIdAttribute) {
    auto doc = parse(R"(<body><span id="hero">text</span></body>)");
    ASSERT_NE(doc, nullptr);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    bool found_id = false;
    for (auto& attr : span->attributes) {
        if (attr.name == "id" && attr.value == "hero") {
            found_id = true; break;
        }
    }
    EXPECT_TRUE(found_id);
}

TEST(TreeBuilder, DivWithClassAttribute) {
    auto doc = parse(R"(<body><div class="container">content</div></body>)");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    bool found_class = false;
    for (auto& attr : div->attributes) {
        if (attr.name == "class" && attr.value == "container") {
            found_class = true; break;
        }
    }
    EXPECT_TRUE(found_class);
}

TEST(TreeBuilder, ArticleWithNestedParagraph) {
    auto doc = parse(R"(<body><article><p>story content</p></article></body>)");
    ASSERT_NE(doc, nullptr);
    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "story content");
}

TEST(TreeBuilder, AsideWithText) {
    auto doc = parse(R"(<body><aside>tip text</aside></body>)");
    ASSERT_NE(doc, nullptr);
    auto* aside = doc->find_element("aside");
    ASSERT_NE(aside, nullptr);
    EXPECT_EQ(aside->text_content(), "tip text");
}

TEST(TreeBuilder, NavWithAnchor) {
    auto doc = parse(R"(<body><nav><a href="/about">About</a></nav></body>)");
    ASSERT_NE(doc, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->text_content(), "About");
}

TEST(TreeBuilder, FooterWithCopyrightText) {
    auto doc = parse(R"(<body><footer>Copyright 2024</footer></body>)");
    ASSERT_NE(doc, nullptr);
    auto* footer = doc->find_element("footer");
    ASSERT_NE(footer, nullptr);
    EXPECT_EQ(footer->text_content(), "Copyright 2024");
}

// ============================================================================
// Cycle 671: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, HeaderContainingSiteTitle) {
    auto doc = parse(R"(<body><header><h1>Site Title</h1></header></body>)");
    ASSERT_NE(doc, nullptr);
    auto* header = doc->find_element("header");
    ASSERT_NE(header, nullptr);
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Site Title");
}

TEST(TreeBuilder, MainContainingParagraph) {
    auto doc = parse(R"(<body><main><p>Main content</p></main></body>)");
    ASSERT_NE(doc, nullptr);
    auto* main_el = doc->find_element("main");
    ASSERT_NE(main_el, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Main content");
}

TEST(TreeBuilder, SectionContainingParagraph) {
    auto doc = parse(R"(<body><section><p>Section text</p></section></body>)");
    ASSERT_NE(doc, nullptr);
    auto* section = doc->find_element("section");
    ASSERT_NE(section, nullptr);
}

TEST(TreeBuilder, HrBetweenParagraphs) {
    auto doc = parse(R"(<body><p>Before</p><hr><p>After</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
}

TEST(TreeBuilder, BrInsideParagraph) {
    auto doc = parse(R"(<body><p>Line 1<br>Line 2</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* br = doc->find_element("br");
    ASSERT_NE(br, nullptr);
}

TEST(TreeBuilder, ImgElement) {
    auto doc = parse(R"(<body><img src="photo.jpg" alt="Photo"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
}

TEST(TreeBuilder, InputElement) {
    auto doc = parse(R"(<body><form><input type="text" name="q"></form></body>)");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
}

TEST(TreeBuilder, TableWithRows) {
    auto doc = parse(R"(<body><table><tr><td>Cell</td></tr></table></body>)");
    ASSERT_NE(doc, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
}

// ============================================================================
// Cycle 679: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, AnchorHrefText) {
    auto doc = parse(R"(<body><a href="https://example.com">Click here</a></body>)");
    ASSERT_NE(doc, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->text_content(), "Click here");
}

TEST(TreeBuilder, ParagraphWithMultipleWords) {
    auto doc = parse(R"(<body><p>one two three four five</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_NE(p->text_content().find("three"), std::string::npos);
}

TEST(TreeBuilder, H1HeadingText) {
    auto doc = parse(R"(<body><h1>Main Heading</h1></body>)");
    ASSERT_NE(doc, nullptr);
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Main Heading");
}

TEST(TreeBuilder, H2SubHeadingText) {
    auto doc = parse(R"(<body><h2>Sub Heading</h2></body>)");
    ASSERT_NE(doc, nullptr);
    auto* h2 = doc->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->text_content(), "Sub Heading");
}

TEST(TreeBuilder, NestedDivsFound) {
    auto doc = parse(R"(<body><div><div id="inner">content</div></div></body>)");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
}

TEST(TreeBuilder, SpanWithDataAttribute) {
    auto doc = parse(R"(<body><span data-value="42">text</span></body>)");
    ASSERT_NE(doc, nullptr);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    bool found = false;
    for (auto& attr : span->attributes) {
        if (attr.name == "data-value" && attr.value == "42") {
            found = true; break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ButtonWithTypeAttribute) {
    auto doc = parse(R"(<body><button type="submit">Submit</button></body>)");
    ASSERT_NE(doc, nullptr);
    auto* btn = doc->find_element("button");
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->text_content(), "Submit");
}

TEST(TreeBuilder, ParagraphAfterHeading) {
    auto doc = parse(R"(<body><h1>Title</h1><p>Description text</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Description text");
}

// ============================================================================
// Cycle 686: More HTML parser tests
// ============================================================================

TEST(TreeBuilder, H3HeadingText) {
    auto doc = parse(R"(<body><h3>Tertiary Heading</h3></body>)");
    ASSERT_NE(doc, nullptr);
    auto* h3 = doc->find_element("h3");
    ASSERT_NE(h3, nullptr);
    EXPECT_EQ(h3->text_content(), "Tertiary Heading");
}

TEST(TreeBuilder, H4HeadingText) {
    auto doc = parse(R"(<body><h4>Fourth Level</h4></body>)");
    ASSERT_NE(doc, nullptr);
    auto* h4 = doc->find_element("h4");
    ASSERT_NE(h4, nullptr);
    EXPECT_EQ(h4->text_content(), "Fourth Level");
}

TEST(TreeBuilder, FormInputButton) {
    auto doc = parse(R"(<body><form><input type="email"><button>Go</button></form></body>)");
    ASSERT_NE(doc, nullptr);
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
}

TEST(TreeBuilder, ListItemAttributes) {
    auto doc = parse(R"(<body><ul><li class="item">First</li></ul></body>)");
    ASSERT_NE(doc, nullptr);
    auto* li = doc->find_element("li");
    ASSERT_NE(li, nullptr);
    bool found_class = false;
    for (auto& attr : li->attributes) {
        if (attr.name == "class") { found_class = true; break; }
    }
    EXPECT_TRUE(found_class);
}

TEST(TreeBuilder, DivWithMultipleChildren) {
    auto doc = parse(R"(<body><div><p>one</p><p>two</p><p>three</p></div></body>)");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
}

TEST(TreeBuilder, MixedInlineAndBlock) {
    auto doc = parse(R"(<body><p>Hello <strong>world</strong>!</p></body>)");
    ASSERT_NE(doc, nullptr);
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "world");
}

TEST(TreeBuilder, LinkWithMultipleAttributes) {
    auto doc = parse(R"(<body><a href="https://example.com" target="_blank" rel="noopener">Link</a></body>)");
    ASSERT_NE(doc, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    bool found_target = false;
    for (auto& attr : a->attributes) {
        if (attr.name == "target") { found_target = true; break; }
    }
    EXPECT_TRUE(found_target);
}

TEST(TreeBuilder, TwoSiblingParagraphs) {
    auto doc = parse(R"(<body><p>First paragraph</p><p>Second paragraph</p></body>)");
    ASSERT_NE(doc, nullptr);
    // Just verify parsing doesn't crash and body exists
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_NE(p->text_content().find("First"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Cycle 691 — 8 additional HTML tests (input types and form attributes)
// ---------------------------------------------------------------------------

TEST(TreeBuilder, InputTypeNumber) {
    auto doc = parse(R"(<body><input type="number" min="0" max="100"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found_type = false;
    for (auto& attr : input->attributes) {
        if (attr.name == "type" && attr.value == "number") { found_type = true; break; }
    }
    EXPECT_TRUE(found_type);
}

TEST(TreeBuilder, InputTypeRange) {
    auto doc = parse(R"(<body><input type="range" min="0" max="100" step="5"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found_step = false;
    for (auto& attr : input->attributes) {
        if (attr.name == "step" && attr.value == "5") { found_step = true; break; }
    }
    EXPECT_TRUE(found_step);
}

TEST(TreeBuilder, InputTypeDate) {
    auto doc = parse(R"(<body><input type="date" value="2024-01-15"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found_value = false;
    for (auto& attr : input->attributes) {
        if (attr.name == "value" && attr.value == "2024-01-15") { found_value = true; break; }
    }
    EXPECT_TRUE(found_value);
}

TEST(TreeBuilder, InputTypeTel) {
    auto doc = parse(R"(<body><input type="tel" placeholder="+1-123-456-7890"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (auto& attr : input->attributes) {
        if (attr.name == "type" && attr.value == "tel") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, OrderedListWithStart) {
    auto doc = parse(R"(<body><ol start="5"><li>Item A</li><li>Item B</li></ol></body>)");
    ASSERT_NE(doc, nullptr);
    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    bool has_start = false;
    for (auto& attr : ol->attributes) {
        if (attr.name == "start" && attr.value == "5") { has_start = true; break; }
    }
    EXPECT_TRUE(has_start);
}

TEST(TreeBuilder, BlockquoteWithCiteAttr) {
    auto doc = parse(R"(<body><blockquote cite="https://example.com">A quote.</blockquote></body>)");
    ASSERT_NE(doc, nullptr);
    auto* bq = doc->find_element("blockquote");
    ASSERT_NE(bq, nullptr);
    bool has_cite = false;
    for (auto& attr : bq->attributes) {
        if (attr.name == "cite") { has_cite = true; break; }
    }
    EXPECT_TRUE(has_cite);
}

TEST(TreeBuilder, InputTypeFile) {
    auto doc = parse(R"(<body><input type="file" accept=".pdf,.doc"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found_type = false;
    for (auto& attr : input->attributes) {
        if (attr.name == "type" && attr.value == "file") { found_type = true; break; }
    }
    EXPECT_TRUE(found_type);
}

TEST(TreeBuilder, OlReversedAttribute) {
    auto doc = parse(R"(<body><ol reversed><li>Three</li><li>Two</li><li>One</li></ol></body>)");
    ASSERT_NE(doc, nullptr);
    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    bool has_reversed = false;
    for (auto& attr : ol->attributes) {
        if (attr.name == "reversed") { has_reversed = true; break; }
    }
    EXPECT_TRUE(has_reversed);
}

// ---------------------------------------------------------------------------
// Cycle 703 — 8 additional HTML tests (more input types and form attributes)
// ---------------------------------------------------------------------------

TEST(TreeBuilder, InputTypePassword) {
    auto doc = parse(R"(<body><input type="password" name="pwd" required></body>)");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (auto& attr : input->attributes) {
        if (attr.name == "type" && attr.value == "password") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeHiddenWithValue) {
    auto doc = parse(R"(<body><input type="hidden" name="csrf" value="abc123"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found_value = false;
    for (auto& attr : input->attributes) {
        if (attr.name == "value" && attr.value == "abc123") { found_value = true; break; }
    }
    EXPECT_TRUE(found_value);
}

TEST(TreeBuilder, InputTypeColorWithDefaultValue) {
    auto doc = parse(R"(<body><input type="color" value="#ff0000"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (auto& attr : input->attributes) {
        if (attr.name == "value" && attr.value == "#ff0000") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeWeekIsParsed) {
    auto doc = parse(R"(<body><input type="week" name="week"></body>)");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (auto& attr : input->attributes) {
        if (attr.name == "type" && attr.value == "week") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, SelectWithMultipleAttribute) {
    auto doc = parse(R"(<body><select multiple name="colors"><option>Red</option></select></body>)");
    ASSERT_NE(doc, nullptr);
    auto* sel = doc->find_element("select");
    ASSERT_NE(sel, nullptr);
    bool found = false;
    for (auto& attr : sel->attributes) {
        if (attr.name == "multiple") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TextareaWithNameAttribute) {
    auto doc = parse(R"(<body><textarea name="message" rows="5" cols="40">Default text</textarea></body>)");
    ASSERT_NE(doc, nullptr);
    auto* ta = doc->find_element("textarea");
    ASSERT_NE(ta, nullptr);
    bool found = false;
    for (auto& attr : ta->attributes) {
        if (attr.name == "name" && attr.value == "message") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, FormWithEnctype) {
    auto doc = parse(R"(<body><form action="/upload" method="post" enctype="multipart/form-data"></form></body>)");
    ASSERT_NE(doc, nullptr);
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    bool found = false;
    for (auto& attr : form->attributes) {
        if (attr.name == "enctype") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ButtonWithDisabledAttribute) {
    auto doc = parse(R"(<body><button disabled type="submit">Submit</button></body>)");
    ASSERT_NE(doc, nullptr);
    auto* btn = doc->find_element("button");
    ASSERT_NE(btn, nullptr);
    bool found = false;
    for (auto& attr : btn->attributes) {
        if (attr.name == "disabled") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ArticleElementIsParsed) {
    auto doc = clever::html::parse("<article><p>Content</p></article>");
    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    EXPECT_EQ(article->tag_name, "article");
}

TEST(TreeBuilder, AsideElementIsParsed) {
    auto doc = clever::html::parse("<aside>sidebar</aside>");
    auto* aside = doc->find_element("aside");
    ASSERT_NE(aside, nullptr);
    EXPECT_EQ(aside->tag_name, "aside");
}

TEST(TreeBuilder, NavElementIsParsed) {
    auto doc = clever::html::parse("<nav><a href='/'>Home</a></nav>");
    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    EXPECT_EQ(nav->tag_name, "nav");
}

TEST(TreeBuilder, FooterElementIsParsed) {
    auto doc = clever::html::parse("<footer>Footer text</footer>");
    auto* footer = doc->find_element("footer");
    ASSERT_NE(footer, nullptr);
    EXPECT_EQ(footer->tag_name, "footer");
}

TEST(TreeBuilder, HeaderElementIsParsed) {
    auto doc = clever::html::parse("<header><h1>Title</h1></header>");
    auto* header = doc->find_element("header");
    ASSERT_NE(header, nullptr);
    EXPECT_EQ(header->tag_name, "header");
}

TEST(TreeBuilder, DataAttrOnDiv) {
    auto doc = clever::html::parse(R"(<div data-id="42">content</div>)");
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    bool found = false;
    for (const auto& attr : div->attributes) {
        if (attr.name == "data-id" && attr.value == "42") found = true;
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, LangAttributeOnHtml) {
    auto doc = clever::html::parse(R"(<html lang="en"><body></body></html>)");
    auto* html_elem = doc->find_element("html");
    ASSERT_NE(html_elem, nullptr);
    bool found = false;
    for (const auto& attr : html_elem->attributes) {
        if (attr.name == "lang" && attr.value == "en") found = true;
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, FigureWithFigcaptionParsed) {
    auto doc = clever::html::parse("<figure><img src='x.png'/><figcaption>Caption</figcaption></figure>");
    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);
    auto* figcaption = doc->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
}

TEST(TreeBuilder, MetaCharsetIsParsed) {
    auto doc = clever::html::parse(R"(<html><head><meta charset="UTF-8"></head></html>)");
    auto* meta = doc->find_element("meta");
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->tag_name, "meta");
}

TEST(TreeBuilder, ScriptElementIsParsed) {
    auto doc = clever::html::parse("<html><head><script>var x = 1;</script></head></html>");
    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    EXPECT_EQ(script->tag_name, "script");
}

TEST(TreeBuilder, StyleElementIsParsed) {
    auto doc = clever::html::parse("<html><head><style>body { color: red; }</style></head></html>");
    auto* style = doc->find_element("style");
    ASSERT_NE(style, nullptr);
    EXPECT_EQ(style->tag_name, "style");
}

TEST(TreeBuilder, MarkElementIsParsed) {
    auto doc = clever::html::parse("<p>Some <mark>highlighted</mark> text</p>");
    auto* mark = doc->find_element("mark");
    ASSERT_NE(mark, nullptr);
    EXPECT_EQ(mark->tag_name, "mark");
}

TEST(TreeBuilder, SectionWithIdAttr) {
    auto doc = clever::html::parse(R"(<section id="main"><p>text</p></section>)");
    auto* section = doc->find_element("section");
    ASSERT_NE(section, nullptr);
    bool found = false;
    for (const auto& attr : section->attributes) {
        if (attr.name == "id" && attr.value == "main") found = true;
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AbbrElementIsParsed) {
    auto doc = clever::html::parse(R"(<abbr title="HyperText Markup Language">HTML</abbr>)");
    auto* abbr = doc->find_element("abbr");
    ASSERT_NE(abbr, nullptr);
    EXPECT_EQ(abbr->tag_name, "abbr");
}

TEST(TreeBuilder, TimeElementIsParsed) {
    auto doc = clever::html::parse(R"(<time datetime="2024-01-01">January 1</time>)");
    auto* time_elem = doc->find_element("time");
    ASSERT_NE(time_elem, nullptr);
    EXPECT_EQ(time_elem->tag_name, "time");
}

TEST(TreeBuilder, VideoElementIsParsed) {
    auto doc = clever::html::parse(R"(<video src="movie.mp4" controls></video>)");
    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    EXPECT_EQ(video->tag_name, "video");
}

TEST(TreeBuilder, InputRequiredAttribute) {
    auto doc = clever::html::parse(R"(<input type="email" required>)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes) {
        if (attr.name == "required") found = true;
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputMinMaxAttributes) {
    auto doc = clever::html::parse(R"(<input type="number" min="0" max="100">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool has_min = false, has_max = false;
    for (const auto& attr : input->attributes) {
        if (attr.name == "min") has_min = true;
        if (attr.name == "max") has_max = true;
    }
    EXPECT_TRUE(has_min && has_max);
}

TEST(TreeBuilder, AudioElementIsParsed) {
    auto doc = clever::html::parse(R"(<audio src="song.mp3" controls></audio>)");
    auto* audio = doc->find_element("audio");
    ASSERT_NE(audio, nullptr);
    EXPECT_EQ(audio->tag_name, "audio");
}

TEST(TreeBuilder, CanvasElementIsParsed) {
    auto doc = clever::html::parse(R"(<canvas width="800" height="600"></canvas>)");
    auto* canvas = doc->find_element("canvas");
    ASSERT_NE(canvas, nullptr);
    EXPECT_EQ(canvas->tag_name, "canvas");
}

TEST(TreeBuilder, IframeElementIsParsed) {
    auto doc = clever::html::parse(R"(<iframe src="https://example.com" title="embed"></iframe>)");
    auto* iframe = doc->find_element("iframe");
    ASSERT_NE(iframe, nullptr);
    EXPECT_EQ(iframe->tag_name, "iframe");
}

TEST(TreeBuilder, DetailsWithSummary) {
    auto doc = clever::html::parse("<details><summary>Toggle</summary><p>Content</p></details>");
    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
}

TEST(TreeBuilder, DialogElementIsParsed) {
    auto doc = clever::html::parse(R"(<dialog open><p>Modal content</p></dialog>)");
    auto* dialog = doc->find_element("dialog");
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->tag_name, "dialog");
}

TEST(TreeBuilder, ProgressElementIsParsed) {
    auto doc = clever::html::parse(R"(<progress value="70" max="100"></progress>)");
    auto* progress = doc->find_element("progress");
    ASSERT_NE(progress, nullptr);
    EXPECT_EQ(progress->tag_name, "progress");
}

TEST(TreeBuilder, TableHeaderCellIsParsed) {
    auto doc = clever::html::parse("<table><tr><th>Header</th><td>Data</td></tr></table>");
    auto* th = doc->find_element("th");
    ASSERT_NE(th, nullptr);
    EXPECT_EQ(th->tag_name, "th");
}

TEST(TreeBuilder, TableBodyCaptionParsed) {
    auto doc = clever::html::parse("<table><caption>My Table</caption><tr><td>cell</td></tr></table>");
    auto* caption = doc->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->tag_name, "caption");
}

TEST(TreeBuilder, NestedUnorderedList) {
    auto doc = clever::html::parse("<ul><li>Item 1<ul><li>Sub-item</li></ul></li></ul>");
    auto all_ul = doc->find_all_elements("ul");
    EXPECT_GE(all_ul.size(), 2u);
}

TEST(TreeBuilder, DefinitionListIsParsed) {
    auto doc = clever::html::parse("<dl><dt>Term</dt><dd>Definition</dd></dl>");
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    auto* dt = doc->find_element("dt");
    ASSERT_NE(dt, nullptr);
    auto* dd = doc->find_element("dd");
    ASSERT_NE(dd, nullptr);
}

TEST(TreeBuilder, CustomDataAttributeParsed) {
    auto doc = clever::html::parse(R"(<span data-user-id="42" data-role="admin">text</span>)");
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    bool has_user_id = false, has_role = false;
    for (const auto& attr : span->attributes) {
        if (attr.name == "data-user-id") has_user_id = true;
        if (attr.name == "data-role") has_role = true;
    }
    EXPECT_TRUE(has_user_id && has_role);
}

TEST(TreeBuilder, MeterElementIsParsed) {
    auto doc = clever::html::parse(R"(<meter value="6" min="0" max="10">6 out of 10</meter>)");
    auto* meter = doc->find_element("meter");
    ASSERT_NE(meter, nullptr);
    EXPECT_EQ(meter->tag_name, "meter");
}

TEST(TreeBuilder, OutputElementIsParsed) {
    auto doc = clever::html::parse(R"(<output for="a b" name="result">0</output>)");
    auto* output = doc->find_element("output");
    ASSERT_NE(output, nullptr);
    EXPECT_EQ(output->tag_name, "output");
}

TEST(TreeBuilder, WbrElementIsParsed) {
    auto doc = clever::html::parse("<p>Very<wbr>LongWord</p>");
    auto* wbr = doc->find_element("wbr");
    ASSERT_NE(wbr, nullptr);
    EXPECT_EQ(wbr->tag_name, "wbr");
}

TEST(TreeBuilder, SvgRectElementIsParsed) {
    auto doc = clever::html::parse(R"(<svg><rect width="100" height="50"/></svg>)");
    auto* rect = doc->find_element("rect");
    ASSERT_NE(rect, nullptr);
    EXPECT_EQ(rect->tag_name, "rect");
}

TEST(TreeBuilder, SvgCircleElementIsParsed) {
    auto doc = clever::html::parse(R"(<svg><circle cx="50" cy="50" r="30"/></svg>)");
    auto* circle = doc->find_element("circle");
    ASSERT_NE(circle, nullptr);
    EXPECT_EQ(circle->tag_name, "circle");
}

TEST(TreeBuilder, SvgPathElementIsParsed) {
    auto doc = clever::html::parse(R"(<svg><path d="M10 10 L90 90"/></svg>)");
    auto* path = doc->find_element("path");
    ASSERT_NE(path, nullptr);
    EXPECT_EQ(path->tag_name, "path");
}

TEST(TreeBuilder, SvgTextElementIsParsed) {
    auto doc = clever::html::parse(R"(<svg><text x="10" y="20">SVG Text</text></svg>)");
    auto* text_elem = doc->find_element("text");
    ASSERT_NE(text_elem, nullptr);
    EXPECT_EQ(text_elem->tag_name, "text");
}

TEST(TreeBuilder, EmbedElementIsParsed) {
    auto doc = clever::html::parse(R"(<embed type="application/pdf" src="file.pdf">)");
    auto* embed = doc->find_element("embed");
    ASSERT_NE(embed, nullptr);
    EXPECT_EQ(embed->tag_name, "embed");
}

TEST(TreeBuilder, ObjectElementIsParsed) {
    auto doc = clever::html::parse(R"(<object type="image/png" data="img.png"></object>)");
    auto* obj = doc->find_element("object");
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->tag_name, "object");
}

TEST(TreeBuilder, SourceElementInVideo) {
    auto doc = clever::html::parse(R"(<video><source src="movie.mp4" type="video/mp4"></video>)");
    auto* source = doc->find_element("source");
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->tag_name, "source");
}

TEST(TreeBuilder, TrackElementInVideo) {
    auto doc = clever::html::parse(R"(<video><track kind="subtitles" src="subs.vtt"></video>)");
    auto* track = doc->find_element("track");
    ASSERT_NE(track, nullptr);
    EXPECT_EQ(track->tag_name, "track");
}

// Cycle 760 — HTML picture, map, col, and interactive elements
TEST(TreeBuilder, PictureElementIsParsed) {
    auto doc = clever::html::parse(R"(<picture><img src="photo.jpg" alt="Photo"></picture>)");
    auto* picture = doc->find_element("picture");
    ASSERT_NE(picture, nullptr);
    EXPECT_EQ(picture->tag_name, "picture");
}

TEST(TreeBuilder, MapElementIsParsed) {
    auto doc = clever::html::parse(R"(<map name="nav"><area shape="rect" coords="0,0,100,100" href="#"></map>)");
    auto* map = doc->find_element("map");
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map->tag_name, "map");
}

TEST(TreeBuilder, AreaElementInMap) {
    auto doc = clever::html::parse(R"(<map name="nav"><area shape="circle" coords="50,50,30" href="#"></map>)");
    auto* area = doc->find_element("area");
    ASSERT_NE(area, nullptr);
    EXPECT_EQ(area->tag_name, "area");
}

TEST(TreeBuilder, ColGroupIsParsed) {
    auto doc = clever::html::parse(R"(<table><colgroup><col span="2"></colgroup></table>)");
    auto* colgroup = doc->find_element("colgroup");
    ASSERT_NE(colgroup, nullptr);
    EXPECT_EQ(colgroup->tag_name, "colgroup");
}

TEST(TreeBuilder, ColElementIsParsed) {
    auto doc = clever::html::parse(R"(<table><colgroup><col span="3"></colgroup></table>)");
    auto* col = doc->find_element("col");
    ASSERT_NE(col, nullptr);
    EXPECT_EQ(col->tag_name, "col");
}

TEST(TreeBuilder, SlotElementIsParsed) {
    auto doc = clever::html::parse(R"(<slot name="header">default</slot>)");
    auto* slot = doc->find_element("slot");
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->tag_name, "slot");
}

TEST(TreeBuilder, RubyAnnotationIsParsed) {
    auto doc = clever::html::parse(R"(<ruby>漢<rt>かん</rt></ruby>)");
    auto* ruby = doc->find_element("ruby");
    ASSERT_NE(ruby, nullptr);
    EXPECT_EQ(ruby->tag_name, "ruby");
}

TEST(TreeBuilder, RubyRtElementIsParsed) {
    auto doc = clever::html::parse(R"(<ruby>字<rt>ji</rt></ruby>)");
    auto* rt = doc->find_element("rt");
    ASSERT_NE(rt, nullptr);
    EXPECT_EQ(rt->tag_name, "rt");
}

// Cycle 767 — HTML form advanced elements
TEST(TreeBuilder, OptgroupElementIsParsed) {
    auto doc = clever::html::parse(R"(<select><optgroup label="Group A"><option>A1</option></optgroup></select>)");
    auto* optgroup = doc->find_element("optgroup");
    ASSERT_NE(optgroup, nullptr);
    EXPECT_EQ(optgroup->tag_name, "optgroup");
}

TEST(TreeBuilder, SelectOptgroupLabelAttr) {
    auto doc = clever::html::parse(R"(<select><optgroup label="Colors"><option>Red</option></optgroup></select>)");
    auto* optgroup = doc->find_element("optgroup");
    ASSERT_NE(optgroup, nullptr);
    bool found = false;
    for (const auto& attr : optgroup->attributes)
        if (attr.name == "label" && attr.value == "Colors") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, FormNovalidateAttr) {
    auto doc = clever::html::parse(R"(<form novalidate action="/submit"></form>)");
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    bool found = false;
    for (const auto& attr : form->attributes)
        if (attr.name == "novalidate") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputAutocompleteAttr) {
    auto doc = clever::html::parse(R"(<input type="text" autocomplete="email">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "autocomplete" && attr.value == "email") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputAutofocusAttr) {
    auto doc = clever::html::parse(R"(<input type="search" autofocus>)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "autofocus") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ButtonFormActionAttr) {
    auto doc = clever::html::parse(R"(<button formaction="/override" type="submit">Go</button>)");
    auto* btn = doc->find_element("button");
    ASSERT_NE(btn, nullptr);
    bool found = false;
    for (const auto& attr : btn->attributes)
        if (attr.name == "formaction" && attr.value == "/override") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputPatternAttr) {
    auto doc = clever::html::parse(R"(<input type="text" pattern="[A-Z]{3}">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "pattern") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputListAttr) {
    auto doc = clever::html::parse(R"(<input type="text" list="suggestions"><datalist id="suggestions"></datalist>)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "list" && attr.value == "suggestions") found = true;
    EXPECT_TRUE(found);
}

// Cycle 775 — HTML structural and modern elements
TEST(TreeBuilder, HGroupElementIsParsed) {
    auto doc = clever::html::parse(R"(<hgroup><h1>Title</h1><p>Tagline</p></hgroup>)");
    auto* hgroup = doc->find_element("hgroup");
    ASSERT_NE(hgroup, nullptr);
    EXPECT_EQ(hgroup->tag_name, "hgroup");
}

TEST(TreeBuilder, SearchElementIsParsedV2) {
    auto doc = clever::html::parse(R"(<search><form><input type="search"></form></search>)");
    auto* search = doc->find_element("search");
    ASSERT_NE(search, nullptr);
    EXPECT_EQ(search->tag_name, "search");
}

TEST(TreeBuilder, MenuElementIsParsed) {
    auto doc = clever::html::parse(R"(<menu><li>Item 1</li><li>Item 2</li></menu>)");
    auto* menu = doc->find_element("menu");
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(menu->tag_name, "menu");
}

TEST(TreeBuilder, SummaryElementIsParsed) {
    auto doc = clever::html::parse(R"(<details><summary>Click me</summary><p>Content</p></details>)");
    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->tag_name, "summary");
}

TEST(TreeBuilder, PreFormattedElement) {
    auto doc = clever::html::parse(R"(<pre>  code  here  </pre>)");
    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
    EXPECT_EQ(pre->tag_name, "pre");
}

TEST(TreeBuilder, ScriptTypeModuleAttr) {
    auto doc = clever::html::parse(R"(<script type="module" src="app.js"></script>)");
    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    bool found = false;
    for (const auto& attr : script->attributes)
        if (attr.name == "type" && attr.value == "module") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, BlockquoteCiteAttr) {
    auto doc = clever::html::parse(R"(<blockquote cite="https://example.com"><p>Quote</p></blockquote>)");
    auto* bq = doc->find_element("blockquote");
    ASSERT_NE(bq, nullptr);
    bool found = false;
    for (const auto& attr : bq->attributes)
        if (attr.name == "cite") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MathElementIsParsed) {
    auto doc = clever::html::parse(R"(<math><mi>x</mi><mo>+</mo><mn>1</mn></math>)");
    auto* math = doc->find_element("math");
    ASSERT_NE(math, nullptr);
    EXPECT_EQ(math->tag_name, "math");
}
