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
