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
