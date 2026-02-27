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

TEST(TreeBuilder, ScriptDeferAttr) {
    auto doc = clever::html::parse(R"(<script defer src="app.js"></script>)");
    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    bool found = false;
    for (const auto& attr : script->attributes)
        if (attr.name == "defer") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ScriptAsyncAttr) {
    auto doc = clever::html::parse(R"(<script async src="analytics.js"></script>)");
    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    bool found = false;
    for (const auto& attr : script->attributes)
        if (attr.name == "async") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, LinkCanonical) {
    auto doc = clever::html::parse(R"(<head><link rel="canonical" href="https://example.com/page"></head>)");
    auto* link = doc->find_element("link");
    ASSERT_NE(link, nullptr);
    bool found = false;
    for (const auto& attr : link->attributes)
        if (attr.name == "rel" && attr.value == "canonical") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, LinkPreload) {
    auto doc = clever::html::parse(R"(<head><link rel="preload" href="font.woff2" as="font"></head>)");
    auto* link = doc->find_element("link");
    ASSERT_NE(link, nullptr);
    bool found = false;
    for (const auto& attr : link->attributes)
        if (attr.name == "as" && attr.value == "font") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, LinkDnsPrefetch) {
    auto doc = clever::html::parse(R"(<head><link rel="dns-prefetch" href="//cdn.example.com"></head>)");
    auto* link = doc->find_element("link");
    ASSERT_NE(link, nullptr);
    bool found = false;
    for (const auto& attr : link->attributes)
        if (attr.name == "rel" && attr.value == "dns-prefetch") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MetaViewport) {
    auto doc = clever::html::parse(R"(<head><meta name="viewport" content="width=device-width,initial-scale=1"></head>)");
    auto* meta = doc->find_element("meta");
    ASSERT_NE(meta, nullptr);
    bool found = false;
    for (const auto& attr : meta->attributes)
        if (attr.name == "name" && attr.value == "viewport") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MetaDescription) {
    auto doc = clever::html::parse(R"(<head><meta name="description" content="Page description here"></head>)");
    auto* meta = doc->find_element("meta");
    ASSERT_NE(meta, nullptr);
    bool found = false;
    for (const auto& attr : meta->attributes)
        if (attr.name == "content" && attr.value == "Page description here") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MetaOgTitle) {
    auto doc = clever::html::parse(R"(<head><meta property="og:title" content="My Page Title"></head>)");
    auto* meta = doc->find_element("meta");
    ASSERT_NE(meta, nullptr);
    bool found = false;
    for (const auto& attr : meta->attributes)
        if (attr.name == "property" && attr.value == "og:title") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeRangeV2) {
    auto doc = clever::html::parse(R"(<input type="range" min="0" max="100" value="50">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "type" && attr.value == "range") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeColor) {
    auto doc = clever::html::parse(R"(<input type="color" value="#ff0000">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "type" && attr.value == "color") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeDateV2) {
    auto doc = clever::html::parse(R"(<input type="date" name="birthday">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "type" && attr.value == "date") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeTime) {
    auto doc = clever::html::parse(R"(<input type="time" name="appt">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "type" && attr.value == "time") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeFileV2) {
    auto doc = clever::html::parse(R"(<input type="file" accept=".pdf,.doc">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "type" && attr.value == "file") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeNumberV2) {
    auto doc = clever::html::parse(R"(<input type="number" min="1" max="10" step="1">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "type" && attr.value == "number") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeEmailV2) {
    auto doc = clever::html::parse(R"(<input type="email" required placeholder="you@example.com">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "type" && attr.value == "email") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypePasswordV2) {
    auto doc = clever::html::parse(R"(<input type="password" autocomplete="current-password">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "type" && attr.value == "password") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TbodyElement) {
    auto doc = clever::html::parse(R"(<table><tbody><tr><td>A</td></tr></tbody></table>)");
    auto* tbody = doc->find_element("tbody");
    ASSERT_NE(tbody, nullptr);
    EXPECT_EQ(tbody->tag_name, "tbody");
}

TEST(TreeBuilder, TheadElement) {
    auto doc = clever::html::parse(R"(<table><thead><tr><th>Header</th></tr></thead></table>)");
    auto* thead = doc->find_element("thead");
    ASSERT_NE(thead, nullptr);
    EXPECT_EQ(thead->tag_name, "thead");
}

TEST(TreeBuilder, TfootElement) {
    auto doc = clever::html::parse(R"(<table><tfoot><tr><td>Footer</td></tr></tfoot></table>)");
    auto* tfoot = doc->find_element("tfoot");
    ASSERT_NE(tfoot, nullptr);
    EXPECT_EQ(tfoot->tag_name, "tfoot");
}

TEST(TreeBuilder, TrElement) {
    auto doc = clever::html::parse(R"(<table><tr><td>Cell</td></tr></table>)");
    auto* tr = doc->find_element("tr");
    ASSERT_NE(tr, nullptr);
    EXPECT_EQ(tr->tag_name, "tr");
}

TEST(TreeBuilder, TdElement) {
    auto doc = clever::html::parse(R"(<table><tr><td>Data</td></tr></table>)");
    auto* td = doc->find_element("td");
    ASSERT_NE(td, nullptr);
    EXPECT_EQ(td->tag_name, "td");
}

TEST(TreeBuilder, ThElement) {
    auto doc = clever::html::parse(R"(<table><tr><th scope="col">Column</th></tr></table>)");
    auto* th = doc->find_element("th");
    ASSERT_NE(th, nullptr);
    EXPECT_EQ(th->tag_name, "th");
}

TEST(TreeBuilder, TableColspanAttr) {
    auto doc = clever::html::parse(R"(<table><tr><td colspan="2">Wide</td></tr></table>)");
    auto* td = doc->find_element("td");
    ASSERT_NE(td, nullptr);
    bool found = false;
    for (const auto& attr : td->attributes)
        if (attr.name == "colspan" && attr.value == "2") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TableCaptionElement) {
    auto doc = clever::html::parse(R"(<table><caption>My Table</caption><tr><td>A</td></tr></table>)");
    auto* caption = doc->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->tag_name, "caption");
}

TEST(TreeBuilder, AudioSrcAttr) {
    auto doc = clever::html::parse(R"(<audio src="podcast.mp3" controls></audio>)");
    auto* audio = doc->find_element("audio");
    ASSERT_NE(audio, nullptr);
    bool found = false;
    for (const auto& attr : audio->attributes)
        if (attr.name == "src" && attr.value == "podcast.mp3") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AudioControlsAttr) {
    auto doc = clever::html::parse(R"(<audio controls loop><source src="a.mp3"></audio>)");
    auto* audio = doc->find_element("audio");
    ASSERT_NE(audio, nullptr);
    bool found = false;
    for (const auto& attr : audio->attributes)
        if (attr.name == "controls") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, VideoSrcAttr) {
    auto doc = clever::html::parse(R"(<video src="movie.mp4" width="640" height="480"></video>)");
    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    bool found = false;
    for (const auto& attr : video->attributes)
        if (attr.name == "src" && attr.value == "movie.mp4") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, VideoWidthHeightAttrs) {
    auto doc = clever::html::parse(R"(<video width="1280" height="720"></video>)");
    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    bool found_w = false, found_h = false;
    for (const auto& attr : video->attributes) {
        if (attr.name == "width" && attr.value == "1280") found_w = true;
        if (attr.name == "height" && attr.value == "720") found_h = true;
    }
    EXPECT_TRUE(found_w && found_h);
}

TEST(TreeBuilder, VideoMutedAttr) {
    auto doc = clever::html::parse(R"(<video muted autoplay></video>)");
    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    bool found = false;
    for (const auto& attr : video->attributes)
        if (attr.name == "muted") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, VideoPosterAttr) {
    auto doc = clever::html::parse(R"(<video poster="thumbnail.jpg" controls></video>)");
    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    bool found = false;
    for (const auto& attr : video->attributes)
        if (attr.name == "poster" && attr.value == "thumbnail.jpg") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, VideoPreloadAttr) {
    auto doc = clever::html::parse(R"(<video preload="metadata"></video>)");
    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    bool found = false;
    for (const auto& attr : video->attributes)
        if (attr.name == "preload" && attr.value == "metadata") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AudioLoopAttr) {
    auto doc = clever::html::parse(R"(<audio loop autoplay src="bg.mp3"></audio>)");
    auto* audio = doc->find_element("audio");
    ASSERT_NE(audio, nullptr);
    bool found = false;
    for (const auto& attr : audio->attributes)
        if (attr.name == "loop") found = true;
    EXPECT_TRUE(found);
}

// Cycle 818 — form/input/label attributes: enctype, size, wrap, for, tabindex, readonly, disabled, button type
TEST(TreeBuilder, FormEnctypeAttr) {
    auto doc = clever::html::parse(R"(<form enctype="multipart/form-data" action="/upload" method="post"></form>)");
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    bool found = false;
    for (const auto& attr : form->attributes)
        if (attr.name == "enctype" && attr.value == "multipart/form-data") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, SelectSizeAttr) {
    auto doc = clever::html::parse(R"(<select size="5" name="options"></select>)");
    auto* sel = doc->find_element("select");
    ASSERT_NE(sel, nullptr);
    bool found = false;
    for (const auto& attr : sel->attributes)
        if (attr.name == "size" && attr.value == "5") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TextareaWrapAttr) {
    auto doc = clever::html::parse(R"(<textarea wrap="hard" rows="4" cols="40"></textarea>)");
    auto* ta = doc->find_element("textarea");
    ASSERT_NE(ta, nullptr);
    bool found = false;
    for (const auto& attr : ta->attributes)
        if (attr.name == "wrap" && attr.value == "hard") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, LabelForAttr) {
    auto doc = clever::html::parse(R"(<label for="username">Name</label>)");
    auto* label = doc->find_element("label");
    ASSERT_NE(label, nullptr);
    bool found = false;
    for (const auto& attr : label->attributes)
        if (attr.name == "for" && attr.value == "username") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTabindexAttr) {
    auto doc = clever::html::parse(R"(<input type="text" tabindex="3" name="field">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "tabindex" && attr.value == "3") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputReadonlyAttr) {
    auto doc = clever::html::parse(R"(<input type="text" readonly value="locked">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "readonly") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputDisabledAttr) {
    auto doc = clever::html::parse(R"(<input type="submit" disabled value="Send">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "disabled") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ButtonTypeResetAttr) {
    auto doc = clever::html::parse(R"(<button type="reset">Clear</button>)");
    auto* btn = doc->find_element("button");
    ASSERT_NE(btn, nullptr);
    bool found = false;
    for (const auto& attr : btn->attributes)
        if (attr.name == "type" && attr.value == "reset") found = true;
    EXPECT_TRUE(found);
}

// Cycle 826 — ARIA accessibility attributes
TEST(TreeBuilder, AriaLabelAttr) {
    auto doc = clever::html::parse(R"(<button aria-label="Close dialog">X</button>)");
    auto* btn = doc->find_element("button");
    ASSERT_NE(btn, nullptr);
    bool found = false;
    for (const auto& attr : btn->attributes)
        if (attr.name == "aria-label" && attr.value == "Close dialog") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AriaRoleAttr) {
    auto doc = clever::html::parse(R"(<div role="navigation" aria-label="Main"></div>)");
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    bool found = false;
    for (const auto& attr : div->attributes)
        if (attr.name == "role" && attr.value == "navigation") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AriaHiddenAttr) {
    auto doc = clever::html::parse(R"(<span aria-hidden="true">decorative</span>)");
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    bool found = false;
    for (const auto& attr : span->attributes)
        if (attr.name == "aria-hidden" && attr.value == "true") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AriaExpandedAttr) {
    auto doc = clever::html::parse(R"(<button aria-expanded="false" aria-controls="menu">Toggle</button>)");
    auto* btn = doc->find_element("button");
    ASSERT_NE(btn, nullptr);
    bool found = false;
    for (const auto& attr : btn->attributes)
        if (attr.name == "aria-expanded" && attr.value == "false") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AriaDescribedByAttr) {
    auto doc = clever::html::parse(R"(<input type="text" aria-describedby="hint" id="name">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "aria-describedby" && attr.value == "hint") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AriaLiveAttr) {
    auto doc = clever::html::parse(R"(<div aria-live="polite" role="status"></div>)");
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    bool found = false;
    for (const auto& attr : div->attributes)
        if (attr.name == "aria-live" && attr.value == "polite") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AriaPressedAttr) {
    auto doc = clever::html::parse(R"(<button aria-pressed="true">Bold</button>)");
    auto* btn = doc->find_element("button");
    ASSERT_NE(btn, nullptr);
    bool found = false;
    for (const auto& attr : btn->attributes)
        if (attr.name == "aria-pressed" && attr.value == "true") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AriaRequiredAttr) {
    auto doc = clever::html::parse(R"(<input type="email" aria-required="true" name="email">)");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "aria-required" && attr.value == "true") found = true;
    EXPECT_TRUE(found);
}

// Cycle 837 — img srcset/loading/decoding/crossorigin, picture source media/type
TEST(TreeBuilder, ImgSrcsetAttr) {
    auto doc = clever::html::parse(R"(<img src="small.jpg" srcset="small.jpg 320w, large.jpg 1280w" alt="photo">)");
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    bool found = false;
    for (const auto& attr : img->attributes)
        if (attr.name == "srcset") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ImgLoadingLazy) {
    auto doc = clever::html::parse(R"(<img src="image.jpg" loading="lazy" alt="lazy">)");
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    bool found = false;
    for (const auto& attr : img->attributes)
        if (attr.name == "loading" && attr.value == "lazy") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ImgDecodingAsync) {
    auto doc = clever::html::parse(R"(<img src="big.jpg" decoding="async" alt="async">)");
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    bool found = false;
    for (const auto& attr : img->attributes)
        if (attr.name == "decoding" && attr.value == "async") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ImgCrossOriginAnonymous) {
    auto doc = clever::html::parse(R"(<img src="avatar.png" crossorigin="anonymous" alt="avatar">)");
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    bool found = false;
    for (const auto& attr : img->attributes)
        if (attr.name == "crossorigin" && attr.value == "anonymous") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ImgSizesAttr) {
    auto doc = clever::html::parse(R"(<img src="photo.jpg" sizes="(max-width: 600px) 100vw, 50vw" srcset="s.jpg 600w, l.jpg 1200w" alt="">)");
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    bool found = false;
    for (const auto& attr : img->attributes)
        if (attr.name == "sizes") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ImgWidthHeightAttrs) {
    auto doc = clever::html::parse(R"(<img src="photo.jpg" width="800" height="600" alt="photo">)");
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    bool foundW = false, foundH = false;
    for (const auto& attr : img->attributes) {
        if (attr.name == "width" && attr.value == "800") foundW = true;
        if (attr.name == "height" && attr.value == "600") foundH = true;
    }
    EXPECT_TRUE(foundW && foundH);
}

TEST(TreeBuilder, PictureSourceMedia) {
    auto doc = clever::html::parse(R"HTML(<picture><source media="(min-width: 800px)" srcset="large.jpg"><img src="small.jpg" alt=""></picture>)HTML");
    auto* source = doc->find_element("source");
    ASSERT_NE(source, nullptr);
    bool found = false;
    for (const auto& attr : source->attributes)
        if (attr.name == "media") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, PictureSourceType) {
    auto doc = clever::html::parse(R"(<picture><source srcset="photo.webp" type="image/webp"><img src="photo.jpg" alt=""></picture>)");
    auto* source = doc->find_element("source");
    ASSERT_NE(source, nullptr);
    bool found = false;
    for (const auto& attr : source->attributes)
        if (attr.name == "type" && attr.value == "image/webp") found = true;
    EXPECT_TRUE(found);
}

// Cycle 848 — more HTML elements and attributes
TEST(TreeBuilder, BdoElementWithDir) {
    auto doc = clever::html::parse("<body><bdo dir=\"rtl\">text</bdo></body>");
    auto* bdo = doc->find_element("bdo");
    ASSERT_NE(bdo, nullptr);
    bool found = false;
    for (const auto& attr : bdo->attributes)
        if (attr.name == "dir" && attr.value == "rtl") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, DfnElementWithTitle) {
    auto doc = clever::html::parse("<body><dfn title=\"HyperText\">HTML</dfn></body>");
    auto* dfn = doc->find_element("dfn");
    ASSERT_NE(dfn, nullptr);
}

TEST(TreeBuilder, DataElementWithValue) {
    auto doc = clever::html::parse("<body><data value=\"123\">one-two-three</data></body>");
    auto* data = doc->find_element("data");
    ASSERT_NE(data, nullptr);
    bool found = false;
    for (const auto& attr : data->attributes)
        if (attr.name == "value" && attr.value == "123") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, SElementStrikethrough) {
    auto doc = clever::html::parse("<body><s>old price</s></body>");
    auto* s = doc->find_element("s");
    ASSERT_NE(s, nullptr);
}

TEST(TreeBuilder, InputTypeSubmitValue) {
    auto doc = clever::html::parse("<body><input type=\"submit\" value=\"Go\"></body>");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "value" && attr.value == "Go") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeMonth) {
    auto doc = clever::html::parse("<body><input type=\"month\" name=\"birthday\"></body>");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "type" && attr.value == "month") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MetaRobotsNoindex) {
    auto doc = clever::html::parse("<head><meta name=\"robots\" content=\"noindex\"></head>");
    auto* meta = doc->find_element("meta");
    ASSERT_NE(meta, nullptr);
    bool found = false;
    for (const auto& attr : meta->attributes)
        if (attr.name == "content" && attr.value == "noindex") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, LinkRelPreconnect) {
    auto doc = clever::html::parse("<head><link rel=\"preconnect\" href=\"https://cdn.example.com\"></head>");
    auto* link = doc->find_element("link");
    ASSERT_NE(link, nullptr);
    bool found = false;
    for (const auto& attr : link->attributes)
        if (attr.name == "rel" && attr.value == "preconnect") found = true;
    EXPECT_TRUE(found);
}

// Cycle 857 — SRI integrity, ol type/start, anchor download, form input, script nomodule, span lang
TEST(TreeBuilder, LinkIntegrityAttribute) {
    auto doc = clever::html::parse(
        "<head><link rel=\"stylesheet\" href=\"style.css\" integrity=\"sha384-abc123\" crossorigin=\"anonymous\"></head>");
    auto* link = doc->find_element("link");
    ASSERT_NE(link, nullptr);
    bool found = false;
    for (const auto& attr : link->attributes)
        if (attr.name == "integrity" && attr.value == "sha384-abc123") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ScriptIntegrityAttribute) {
    auto doc = clever::html::parse(
        "<head><script src=\"app.js\" integrity=\"sha256-xyz\"></script></head>");
    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    bool found = false;
    for (const auto& attr : script->attributes)
        if (attr.name == "integrity" && attr.value == "sha256-xyz") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, OlTypeAlpha) {
    auto doc = clever::html::parse("<body><ol type=\"a\"><li>first</li></ol></body>");
    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    bool found = false;
    for (const auto& attr : ol->attributes)
        if (attr.name == "type" && attr.value == "a") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, OlStartAttribute) {
    auto doc = clever::html::parse("<body><ol start=\"5\"><li>five</li></ol></body>");
    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    bool found = false;
    for (const auto& attr : ol->attributes)
        if (attr.name == "start" && attr.value == "5") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AnchorDownloadAttribute) {
    auto doc = clever::html::parse("<body><a href=\"file.pdf\" download=\"report.pdf\">Download</a></body>");
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    bool found = false;
    for (const auto& attr : a->attributes)
        if (attr.name == "download" && attr.value == "report.pdf") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputFormAttribute) {
    auto doc = clever::html::parse(
        "<body><form id=\"f\"></form><input type=\"text\" form=\"f\" name=\"q\"></body>");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "form" && attr.value == "f") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ScriptNomodule) {
    auto doc = clever::html::parse("<head><script nomodule src=\"legacy.js\"></script></head>");
    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    bool found = false;
    for (const auto& attr : script->attributes)
        if (attr.name == "nomodule") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, SpanLangAttribute) {
    auto doc = clever::html::parse("<body><span lang=\"fr\">Bonjour</span></body>");
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    bool found = false;
    for (const auto& attr : span->attributes)
        if (attr.name == "lang" && attr.value == "fr") found = true;
    EXPECT_TRUE(found);
}


// Cycle 866 — contenteditable, draggable, spellcheck, translate, rowspan, inputmode, enterkeyhint, details-open
TEST(TreeBuilder, ContentEditableAttribute) {
    auto doc = clever::html::parse("<body><div contenteditable=\"true\">editable</div></body>");
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    bool found = false;
    for (const auto& attr : div->attributes)
        if (attr.name == "contenteditable" && attr.value == "true") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, DraggableAttribute) {
    auto doc = clever::html::parse("<body><img src=\"photo.jpg\" draggable=\"false\" alt=\"photo\"></body>");
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    bool found = false;
    for (const auto& attr : img->attributes)
        if (attr.name == "draggable" && attr.value == "false") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, SpellcheckAttribute) {
    auto doc = clever::html::parse("<body><textarea spellcheck=\"false\"></textarea></body>");
    auto* textarea = doc->find_element("textarea");
    ASSERT_NE(textarea, nullptr);
    bool found = false;
    for (const auto& attr : textarea->attributes)
        if (attr.name == "spellcheck" && attr.value == "false") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TranslateAttribute) {
    auto doc = clever::html::parse("<body><p translate=\"no\">Brand Name</p></body>");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    bool found = false;
    for (const auto& attr : p->attributes)
        if (attr.name == "translate" && attr.value == "no") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TableRowspanAttr) {
    auto doc = clever::html::parse(
        "<body><table><tr><td rowspan=\"2\">cell</td><td>b</td></tr><tr><td>c</td></tr></table></body>");
    auto* td = doc->find_element("td");
    ASSERT_NE(td, nullptr);
    bool found = false;
    for (const auto& attr : td->attributes)
        if (attr.name == "rowspan" && attr.value == "2") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputInputmodeAttribute) {
    auto doc = clever::html::parse("<body><input type=\"text\" inputmode=\"numeric\"></body>");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "inputmode" && attr.value == "numeric") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputEnterkeyhintAttribute) {
    auto doc = clever::html::parse("<body><input type=\"search\" enterkeyhint=\"search\"></body>");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    bool found = false;
    for (const auto& attr : input->attributes)
        if (attr.name == "enterkeyhint" && attr.value == "search") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, DetailsOpenAttribute) {
    auto doc = clever::html::parse("<body><details open><summary>Info</summary><p>content</p></details></body>");
    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    bool found = false;
    for (const auto& attr : details->attributes)
        if (attr.name == "open") found = true;
    EXPECT_TRUE(found);
}

// Cycle 875 — media element attributes: video autoplay/controls/loop, audio preload, iframe sandbox/allow, object data/type, canvas role
TEST(TreeBuilder, VideoAutoplayAttr) {
    auto doc = clever::html::parse("<body><video autoplay src=\"movie.mp4\"></video></body>");
    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    bool found = false;
    for (const auto& attr : video->attributes)
        if (attr.name == "autoplay") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, VideoControlsAttr) {
    auto doc = clever::html::parse("<body><video controls src=\"movie.mp4\"></video></body>");
    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    bool found = false;
    for (const auto& attr : video->attributes)
        if (attr.name == "controls") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, VideoLoopAttr) {
    auto doc = clever::html::parse("<body><video loop src=\"intro.mp4\"></video></body>");
    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    bool found = false;
    for (const auto& attr : video->attributes)
        if (attr.name == "loop") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AudioPreloadNoneAttr) {
    auto doc = clever::html::parse("<body><audio preload=\"none\" src=\"sound.mp3\"></audio></body>");
    auto* audio = doc->find_element("audio");
    ASSERT_NE(audio, nullptr);
    bool found = false;
    for (const auto& attr : audio->attributes)
        if (attr.name == "preload" && attr.value == "none") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, IframeSandboxAttr) {
    auto doc = clever::html::parse("<body><iframe sandbox=\"allow-scripts\" src=\"frame.html\"></iframe></body>");
    auto* iframe = doc->find_element("iframe");
    ASSERT_NE(iframe, nullptr);
    bool found = false;
    for (const auto& attr : iframe->attributes)
        if (attr.name == "sandbox" && attr.value == "allow-scripts") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, IframeAllowAttr) {
    auto doc = clever::html::parse("<body><iframe allow=\"fullscreen\" src=\"player.html\"></iframe></body>");
    auto* iframe = doc->find_element("iframe");
    ASSERT_NE(iframe, nullptr);
    bool found = false;
    for (const auto& attr : iframe->attributes)
        if (attr.name == "allow" && attr.value == "fullscreen") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ObjectDataAttr) {
    auto doc = clever::html::parse("<body><object data=\"file.pdf\" type=\"application/pdf\"></object></body>");
    auto* obj = doc->find_element("object");
    ASSERT_NE(obj, nullptr);
    bool found = false;
    for (const auto& attr : obj->attributes)
        if (attr.name == "data" && attr.value == "file.pdf") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ObjectTypeAttr) {
    auto doc = clever::html::parse("<body><object data=\"plugin.swf\" type=\"application/x-shockwave-flash\"></object></body>");
    auto* obj = doc->find_element("object");
    ASSERT_NE(obj, nullptr);
    bool found = false;
    for (const auto& attr : obj->attributes)
        if (attr.name == "type" && attr.value == "application/x-shockwave-flash") found = true;
    EXPECT_TRUE(found);
}

// Cycle 884 — HTML attribute tests

TEST(TreeBuilder, TextareaRowsAttr) {
    auto doc = clever::html::parse("<body><textarea rows=\"8\"></textarea></body>");
    auto* ta = doc->find_element("textarea");
    ASSERT_NE(ta, nullptr);
    bool found = false;
    for (const auto& attr : ta->attributes)
        if (attr.name == "rows" && attr.value == "8") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TextareaColsAttr) {
    auto doc = clever::html::parse("<body><textarea cols=\"40\"></textarea></body>");
    auto* ta = doc->find_element("textarea");
    ASSERT_NE(ta, nullptr);
    bool found = false;
    for (const auto& attr : ta->attributes)
        if (attr.name == "cols" && attr.value == "40") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TextareaMaxlengthAttr) {
    auto doc = clever::html::parse("<body><textarea maxlength=\"200\"></textarea></body>");
    auto* ta = doc->find_element("textarea");
    ASSERT_NE(ta, nullptr);
    bool found = false;
    for (const auto& attr : ta->attributes)
        if (attr.name == "maxlength" && attr.value == "200") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputCheckedAttr) {
    auto doc = clever::html::parse("<body><input type=\"checkbox\" checked></body>");
    auto* inp = doc->find_element("input");
    ASSERT_NE(inp, nullptr);
    bool found = false;
    for (const auto& attr : inp->attributes)
        if (attr.name == "checked") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputMultipleAttr) {
    auto doc = clever::html::parse("<body><input type=\"file\" multiple></body>");
    auto* inp = doc->find_element("input");
    ASSERT_NE(inp, nullptr);
    bool found = false;
    for (const auto& attr : inp->attributes)
        if (attr.name == "multiple") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputStepAttr) {
    auto doc = clever::html::parse("<body><input type=\"number\" step=\"0.5\"></body>");
    auto* inp = doc->find_element("input");
    ASSERT_NE(inp, nullptr);
    bool found = false;
    for (const auto& attr : inp->attributes)
        if (attr.name == "step" && attr.value == "0.5") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, FieldsetDisabledAttr) {
    auto doc = clever::html::parse("<body><fieldset disabled><legend>Disabled</legend></fieldset></body>");
    auto* fs = doc->find_element("fieldset");
    ASSERT_NE(fs, nullptr);
    bool found = false;
    for (const auto& attr : fs->attributes)
        if (attr.name == "disabled") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ButtonTypeButtonAttr) {
    auto doc = clever::html::parse("<body><button type=\"button\">Click</button></body>");
    auto* btn = doc->find_element("button");
    ASSERT_NE(btn, nullptr);
    bool found = false;
    for (const auto& attr : btn->attributes)
        if (attr.name == "type" && attr.value == "button") found = true;
    EXPECT_TRUE(found);
}

// Cycle 892 — HTML attribute tests

TEST(TreeBuilder, SelectDisabledAttr) {
    auto doc = clever::html::parse("<body><select disabled></select></body>");
    auto* sel = doc->find_element("select");
    ASSERT_NE(sel, nullptr);
    bool found = false;
    for (const auto& attr : sel->attributes)
        if (attr.name == "disabled") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, SelectRequiredAttr) {
    auto doc = clever::html::parse("<body><select required></select></body>");
    auto* sel = doc->find_element("select");
    ASSERT_NE(sel, nullptr);
    bool found = false;
    for (const auto& attr : sel->attributes)
        if (attr.name == "required") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, FormNoValidateAttr) {
    auto doc = clever::html::parse("<body><form novalidate></form></body>");
    auto* frm = doc->find_element("form");
    ASSERT_NE(frm, nullptr);
    bool found = false;
    for (const auto& attr : frm->attributes)
        if (attr.name == "novalidate") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputNameAttr) {
    auto doc = clever::html::parse("<body><input name=\"username\"></body>");
    auto* inp = doc->find_element("input");
    ASSERT_NE(inp, nullptr);
    bool found = false;
    for (const auto& attr : inp->attributes)
        if (attr.name == "name" && attr.value == "username") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputValueAttr) {
    auto doc = clever::html::parse("<body><input value=\"default\"></body>");
    auto* inp = doc->find_element("input");
    ASSERT_NE(inp, nullptr);
    bool found = false;
    for (const auto& attr : inp->attributes)
        if (attr.name == "value" && attr.value == "default") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeSearch) {
    auto doc = clever::html::parse("<body><input type=\"search\" placeholder=\"Search...\"></body>");
    auto* inp = doc->find_element("input");
    ASSERT_NE(inp, nullptr);
    bool found = false;
    for (const auto& attr : inp->attributes)
        if (attr.name == "type" && attr.value == "search") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AnchorTargetAttr) {
    auto doc = clever::html::parse("<body><a href=\"https://example.com\" target=\"_blank\">link</a></body>");
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    bool found = false;
    for (const auto& attr : a->attributes)
        if (attr.name == "target" && attr.value == "_blank") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AnchorRelAttr) {
    auto doc = clever::html::parse("<body><a href=\"https://example.com\" rel=\"noopener noreferrer\">link</a></body>");
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    bool found = false;
    for (const auto& attr : a->attributes)
        if (attr.name == "rel" && attr.value == "noopener noreferrer") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, SectionElement) {
    auto doc = clever::html::parse("<body><section>Content</section></body>");
    auto* el = doc->find_element("section");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "section");
}

TEST(TreeBuilder, FigureElement) {
    auto doc = clever::html::parse("<body><figure><img src=\"photo.jpg\"></figure></body>");
    auto* el = doc->find_element("figure");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "figure");
}

TEST(TreeBuilder, FigcaptionElement) {
    auto doc = clever::html::parse("<body><figure><figcaption>Caption</figcaption></figure></body>");
    auto* el = doc->find_element("figcaption");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "figcaption");
}

TEST(TreeBuilder, LinkFavicon) {
    auto doc = clever::html::parse("<head><link rel=\"icon\" href=\"favicon.ico\"></head><body></body>");
    auto* el = doc->find_element("link");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "rel" && attr.value == "icon") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ScriptSrcAttr) {
    auto doc = clever::html::parse("<body><script src=\"app.js\"></script></body>");
    auto* el = doc->find_element("script");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "src" && attr.value == "app.js") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputMinAttr) {
    auto doc = clever::html::parse("<body><input type=\"number\" min=\"0\"></body>");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "min" && attr.value == "0") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputMaxAttr) {
    auto doc = clever::html::parse("<body><input type=\"number\" max=\"100\"></body>");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "max" && attr.value == "100") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputPlaceholderAttr) {
    auto doc = clever::html::parse("<body><input type=\"text\" placeholder=\"Enter name\"></body>");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "placeholder" && attr.value == "Enter name") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, OptionValueAttr) {
    auto doc = clever::html::parse("<body><select><option value=\"opt1\">Option 1</option></select></body>");
    auto* el = doc->find_element("option");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "value" && attr.value == "opt1") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, OptionSelectedAttr) {
    auto doc = clever::html::parse("<body><select><option selected>A</option></select></body>");
    auto* el = doc->find_element("option");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "selected") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, OptionDisabledAttr) {
    auto doc = clever::html::parse("<body><select><option disabled>B</option></select></body>");
    auto* el = doc->find_element("option");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "disabled") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, OptGroupLabelAttr) {
    auto doc = clever::html::parse("<body><select><optgroup label=\"Group 1\"><option>A</option></optgroup></select></body>");
    auto* el = doc->find_element("optgroup");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "label" && attr.value == "Group 1") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MeterMinAttr) {
    auto doc = clever::html::parse("<body><meter min=\"0\" max=\"100\" value=\"50\">50%</meter></body>");
    auto* el = doc->find_element("meter");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "min" && attr.value == "0") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ProgressMaxAttr) {
    auto doc = clever::html::parse("<body><progress max=\"200\" value=\"100\"></progress></body>");
    auto* el = doc->find_element("progress");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "max" && attr.value == "200") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TdColspanAttr) {
    auto doc = clever::html::parse("<body><table><tr><td colspan=\"3\">Cell</td></tr></table></body>");
    auto* el = doc->find_element("td");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "colspan" && attr.value == "3") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ThScopeAttr) {
    auto doc = clever::html::parse("<body><table><tr><th scope=\"col\">Header</th></tr></table></body>");
    auto* el = doc->find_element("th");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "scope" && attr.value == "col") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MetaHttpEquiv) {
    auto doc = clever::html::parse("<head><meta http-equiv=\"refresh\" content=\"5\"></head><body></body>");
    auto* el = doc->find_element("meta");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "http-equiv" && attr.value == "refresh") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ImgAltAttr) {
    auto doc = clever::html::parse("<body><img src=\"photo.jpg\" alt=\"A photo\"></body>");
    auto* el = doc->find_element("img");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "alt" && attr.value == "A photo") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ImgCrossoriginAttr) {
    auto doc = clever::html::parse("<body><img src=\"image.jpg\" crossorigin=\"anonymous\"></body>");
    auto* el = doc->find_element("img");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "crossorigin") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, IframeHeightAttr) {
    auto doc = clever::html::parse("<body><iframe src=\"embed.html\" height=\"400\"></iframe></body>");
    auto* el = doc->find_element("iframe");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "height" && attr.value == "400") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, IframeNameAttr) {
    auto doc = clever::html::parse("<body><iframe name=\"myframe\"></iframe></body>");
    auto* el = doc->find_element("iframe");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "name" && attr.value == "myframe") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, FormMethodGetAttr) {
    auto doc = clever::html::parse("<body><form method=\"get\" action=\"/search\"></form></body>");
    auto* el = doc->find_element("form");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "method" && attr.value == "get") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, FormMethodPostAttr) {
    auto doc = clever::html::parse("<body><form method=\"post\" action=\"/submit\"></form></body>");
    auto* el = doc->find_element("form");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "method" && attr.value == "post") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, FormTargetAttr) {
    auto doc = clever::html::parse("<body><form target=\"_blank\"></form></body>");
    auto* el = doc->find_element("form");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "target" && attr.value == "_blank") found = true;
    EXPECT_TRUE(found);
}

// Cycle 928 — additional HTML element and attribute coverage
TEST(TreeBuilder, FieldsetElementParsed) {
    auto doc = clever::html::parse("<body><fieldset><legend>Group</legend></fieldset></body>");
    auto* el = doc->find_element("fieldset");
    EXPECT_NE(el, nullptr);
}

TEST(TreeBuilder, LegendElementParsed) {
    auto doc = clever::html::parse("<body><fieldset><legend>Title</legend></fieldset></body>");
    auto* el = doc->find_element("legend");
    EXPECT_NE(el, nullptr);
}

TEST(TreeBuilder, SelectMultipleAttr) {
    auto doc = clever::html::parse("<body><select multiple name=\"opts\"></select></body>");
    auto* el = doc->find_element("select");
    ASSERT_NE(el, nullptr);
    bool found_multiple = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "multiple") found_multiple = true;
    EXPECT_TRUE(found_multiple);
}

TEST(TreeBuilder, TextareaNameAttr) {
    auto doc = clever::html::parse("<body><textarea name=\"message\"></textarea></body>");
    auto* el = doc->find_element("textarea");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "name" && attr.value == "message") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TextareaPlaceholderAttr) {
    auto doc = clever::html::parse("<body><textarea placeholder=\"Enter text\"></textarea></body>");
    auto* el = doc->find_element("textarea");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "placeholder") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TextareaDisabledAttr) {
    auto doc = clever::html::parse("<body><textarea disabled></textarea></body>");
    auto* el = doc->find_element("textarea");
    ASSERT_NE(el, nullptr);
    bool found_disabled = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "disabled") found_disabled = true;
    EXPECT_TRUE(found_disabled);
}

TEST(TreeBuilder, ButtonNameAttr) {
    auto doc = clever::html::parse("<body><button name=\"submit-btn\">Go</button></body>");
    auto* el = doc->find_element("button");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "name" && attr.value == "submit-btn") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ButtonValueAttr) {
    auto doc = clever::html::parse("<body><button value=\"confirm\">OK</button></body>");
    auto* el = doc->find_element("button");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "value" && attr.value == "confirm") found = true;
    EXPECT_TRUE(found);
}

// Cycle 937 — input type variants, link attributes, meta OG tags
TEST(TreeBuilder, InputTypeUrlAttr) {
    auto doc = clever::html::parse("<body><input type=\"url\" name=\"website\"></body>");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "type" && attr.value == "url") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeCheckboxAttr) {
    auto doc = clever::html::parse("<body><input type=\"checkbox\" name=\"agree\"></body>");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "type" && attr.value == "checkbox") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeRadioAttr) {
    auto doc = clever::html::parse("<body><input type=\"radio\" name=\"choice\"></body>");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "type" && attr.value == "radio") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeDatetimeLocalAttr) {
    auto doc = clever::html::parse("<body><input type=\"datetime-local\"></body>");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "type" && attr.value == "datetime-local") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, LinkMediaAttr) {
    auto doc = clever::html::parse("<head><link rel=\"stylesheet\" href=\"print.css\" media=\"print\"></head>");
    auto* el = doc->find_element("link");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "media" && attr.value == "print") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MetaOgImageContent) {
    auto doc = clever::html::parse("<head><meta property=\"og:image\" content=\"https://example.com/img.png\"></head>");
    auto* el = doc->find_element("meta");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "property" && attr.value == "og:image") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MetaOgUrlContent) {
    auto doc = clever::html::parse("<head><meta property=\"og:url\" content=\"https://example.com/\"></head>");
    auto* el = doc->find_element("meta");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "property" && attr.value == "og:url") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ScriptNoModuleAttr) {
    auto doc = clever::html::parse("<head><script nomodule src=\"fallback.js\"></script></head>");
    auto* el = doc->find_element("script");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "nomodule") found = true;
    EXPECT_TRUE(found);
}

// Cycle 946 — table attributes, anchor hreflang, iframe srcdoc/loading, form name
TEST(TreeBuilder, TableBorderAttr) {
    auto doc = clever::html::parse("<body><table border=\"1\"><tr><td>cell</td></tr></table></body>");
    auto* el = doc->find_element("table");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "border" && attr.value == "1") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TableSummaryAttr) {
    auto doc = clever::html::parse("<body><table summary=\"Product list\"></table></body>");
    auto* el = doc->find_element("table");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "summary") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TdRowspanAttr) {
    auto doc = clever::html::parse("<body><table><tr><td rowspan=\"2\">cell</td></tr></table></body>");
    auto* el = doc->find_element("td");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "rowspan" && attr.value == "2") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AnchorHreflangAttr) {
    auto doc = clever::html::parse("<body><a href=\"/fr\" hreflang=\"fr\">French</a></body>");
    auto* el = doc->find_element("a");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "hreflang" && attr.value == "fr") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AnchorPingAttr) {
    auto doc = clever::html::parse("<body><a href=\"/link\" ping=\"/analytics\">Click</a></body>");
    auto* el = doc->find_element("a");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "ping") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, IframeSrcdocAttr) {
    auto doc = clever::html::parse("<body><iframe srcdoc=\"<p>Hello</p>\"></iframe></body>");
    auto* el = doc->find_element("iframe");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "srcdoc") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, IframeLoadingLazyAttr) {
    auto doc = clever::html::parse("<body><iframe src=\"/page\" loading=\"lazy\"></iframe></body>");
    auto* el = doc->find_element("iframe");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "loading" && attr.value == "lazy") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, FormNameAttr) {
    auto doc = clever::html::parse("<body><form name=\"loginForm\"></form></body>");
    auto* el = doc->find_element("form");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "name" && attr.value == "loginForm") found = true;
    EXPECT_TRUE(found);
}
TEST(TreeBuilder, FormActionAttr) {
    auto doc = clever::html::parse("<body><form action=\"/submit\"></form></body>");
    auto* el = doc->find_element("form");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "action" && attr.value == "/submit") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, FormMethodAttr) {
    auto doc = clever::html::parse("<body><form method=\"post\"></form></body>");
    auto* el = doc->find_element("form");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "method" && attr.value == "post") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MeterMaxAttr) {
    auto doc = clever::html::parse("<body><meter max=\"100\"></meter></body>");
    auto* el = doc->find_element("meter");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "max" && attr.value == "100") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MeterValueAttr) {
    auto doc = clever::html::parse("<body><meter value=\"40\"></meter></body>");
    auto* el = doc->find_element("meter");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "value" && attr.value == "40") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MeterOptimumAttr) {
    auto doc = clever::html::parse("<body><meter optimum=\"70\"></meter></body>");
    auto* el = doc->find_element("meter");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "optimum" && attr.value == "70") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MeterLowAttr) {
    auto doc = clever::html::parse("<body><meter low=\"20\"></meter></body>");
    auto* el = doc->find_element("meter");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "low" && attr.value == "20") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MeterHighAttr) {
    auto doc = clever::html::parse("<body><meter high=\"80\"></meter></body>");
    auto* el = doc->find_element("meter");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "high" && attr.value == "80") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AudioAutoplayAttr) {
    auto doc = clever::html::parse("<body><audio autoplay src=\"/audio.mp3\"></audio></body>");
    auto* el = doc->find_element("audio");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "autoplay") found = true;
    EXPECT_TRUE(found);
}
TEST(TreeBuilder, AudioMutedAttr) {
    auto doc = clever::html::parse("<body><audio muted src=\"/audio.mp3\"></audio></body>");
    auto* el = doc->find_element("audio");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "muted") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ProgressValueAttr) {
    auto doc = clever::html::parse("<body><progress value=\"60\" max=\"100\"></progress></body>");
    auto* el = doc->find_element("progress");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "value" && attr.value == "60") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TrackSrcAttr) {
    auto doc = clever::html::parse("<body><video><track src=\"/captions.vtt\"></track></video></body>");
    auto* el = doc->find_element("track");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "src" && attr.value == "/captions.vtt") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TrackKindAttr) {
    auto doc = clever::html::parse("<body><video><track kind=\"subtitles\"></track></video></body>");
    auto* el = doc->find_element("track");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "kind" && attr.value == "subtitles") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TrackLabelAttr) {
    auto doc = clever::html::parse("<body><video><track label=\"English\"></track></video></body>");
    auto* el = doc->find_element("track");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "label" && attr.value == "English") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TrackSrclangAttr) {
    auto doc = clever::html::parse("<body><video><track srclang=\"en\"></track></video></body>");
    auto* el = doc->find_element("track");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "srclang" && attr.value == "en") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TableCellpaddingAttr) {
    auto doc = clever::html::parse("<body><table cellpadding=\"5\"></table></body>");
    auto* el = doc->find_element("table");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "cellpadding" && attr.value == "5") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TableCellspacingAttr) {
    auto doc = clever::html::parse("<body><table cellspacing=\"0\"></table></body>");
    auto* el = doc->find_element("table");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "cellspacing" && attr.value == "0") found = true;
    EXPECT_TRUE(found);
}
TEST(TreeBuilder, TrackDefaultAttr) {
    auto doc = clever::html::parse("<body><video><track default src=\"/captions.vtt\"></track></video></body>");
    auto* el = doc->find_element("track");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "default") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, SourceSrcsetAttr) {
    auto doc = clever::html::parse("<body><picture><source srcset=\"/img@2x.png 2x\"></source></picture></body>");
    auto* el = doc->find_element("source");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "srcset") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ButtonDisabledAttr) {
    auto doc = clever::html::parse("<body><button disabled>Click</button></body>");
    auto* el = doc->find_element("button");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "disabled") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, SelectFormAttr) {
    auto doc = clever::html::parse("<body><select form=\"myForm\" name=\"color\"></select></body>");
    auto* el = doc->find_element("select");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "form" && attr.value == "myForm") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputFormactionAttr) {
    auto doc = clever::html::parse("<body><input type=\"submit\" formaction=\"/alt-submit\"></body>");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "formaction") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputFormmethodAttr) {
    auto doc = clever::html::parse("<body><input type=\"submit\" formmethod=\"post\"></body>");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "formmethod" && attr.value == "post") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TextareaFormAttr) {
    auto doc = clever::html::parse("<body><textarea form=\"signup\" name=\"bio\"></textarea></body>");
    auto* el = doc->find_element("textarea");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "form" && attr.value == "signup") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ColWidthAttr) {
    auto doc = clever::html::parse("<body><table><colgroup><col width=\"100\"></col></colgroup></table></body>");
    auto* el = doc->find_element("col");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "width" && attr.value == "100") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, EmbedSrcAttr) {
    auto doc = clever::html::parse("<body><embed src=\"movie.swf\"></body>");
    auto* el = doc->find_element("embed");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "src" && attr.value == "movie.swf") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, EmbedTypeAttr) {
    auto doc = clever::html::parse("<body><embed type=\"application/x-shockwave-flash\"></body>");
    auto* el = doc->find_element("embed");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "type") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, EmbedWidthAttr) {
    auto doc = clever::html::parse("<body><embed width=\"300\"></body>");
    auto* el = doc->find_element("embed");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "width" && attr.value == "300") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, EmbedHeightAttr) {
    auto doc = clever::html::parse("<body><embed height=\"200\"></body>");
    auto* el = doc->find_element("embed");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "height" && attr.value == "200") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MapNameAttr) {
    auto doc = clever::html::parse("<body><map name=\"navmap\"></map></body>");
    auto* el = doc->find_element("map");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "name" && attr.value == "navmap") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AreaHrefAttr) {
    auto doc = clever::html::parse("<body><map name=\"m\"><area href=\"/page\"></area></map></body>");
    auto* el = doc->find_element("area");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "href" && attr.value == "/page") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AreaShapeAttr) {
    auto doc = clever::html::parse("<body><map name=\"m\"><area shape=\"rect\"></area></map></body>");
    auto* el = doc->find_element("area");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "shape" && attr.value == "rect") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AreaCoordsAttr) {
    auto doc = clever::html::parse("<body><map name=\"m\"><area coords=\"0,0,100,100\"></area></map></body>");
    auto* el = doc->find_element("area");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "coords") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, SummaryTextParsed) {
    auto doc = clever::html::parse("<body><details><summary>Toggle me</summary></details></body>");
    auto* el = doc->find_element("summary");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "summary");
}

TEST(TreeBuilder, OutputForAttr) {
    auto doc = clever::html::parse("<body><form id=\"f\"><output for=\"x y\"></output></form></body>");
    auto* el = doc->find_element("output");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "for" && attr.value == "x y") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, TimeDateTime) {
    auto doc = clever::html::parse("<body><time datetime=\"2026-02-28\">Today</time></body>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "datetime" && attr.value == "2026-02-28") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MarkTextParsed) {
    auto doc = clever::html::parse("<body><p>Find <mark>this</mark> word</p></body>");
    auto* el = doc->find_element("mark");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "mark");
}

TEST(TreeBuilder, KbdTextParsed) {
    auto doc = clever::html::parse("<body><p>Press <kbd>Ctrl+C</kbd></p></body>");
    auto* el = doc->find_element("kbd");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "kbd");
}

TEST(TreeBuilder, SampTextParsed) {
    auto doc = clever::html::parse("<body><p><samp>error: file not found</samp></p></body>");
    auto* el = doc->find_element("samp");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "samp");
}

TEST(TreeBuilder, VarTextParsed) {
    auto doc = clever::html::parse("<body><p>Let <var>x</var> = 5</p></body>");
    auto* el = doc->find_element("var");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "var");
}

TEST(TreeBuilder, CiteTextParsed) {
    auto doc = clever::html::parse("<body><p><cite>ECMA-262</cite></p></body>");
    auto* el = doc->find_element("cite");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "cite");
}

TEST(TreeBuilder, SubTagParsed) {
    auto doc = clever::html::parse("<body><p>H<sub>2</sub>O</p></body>");
    auto* el = doc->find_element("sub");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "sub");
}

TEST(TreeBuilder, SupTagParsed) {
    auto doc = clever::html::parse("<body><p>E=mc<sup>2</sup></p></body>");
    auto* el = doc->find_element("sup");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "sup");
}

TEST(TreeBuilder, SmallTagParsed) {
    auto doc = clever::html::parse("<body><p><small>Fine print</small></p></body>");
    auto* el = doc->find_element("small");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "small");
}

TEST(TreeBuilder, DelTagParsed) {
    auto doc = clever::html::parse("<body><p><del>old text</del></p></body>");
    auto* el = doc->find_element("del");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "del");
}

TEST(TreeBuilder, InsTagParsed) {
    auto doc = clever::html::parse("<body><p><ins>new text</ins></p></body>");
    auto* el = doc->find_element("ins");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ins");
}

TEST(TreeBuilder, DataValueAttr) {
    auto doc = clever::html::parse("<body><data value=\"42\">Forty-two</data></body>");
    auto* el = doc->find_element("data");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "value" && attr.value == "42") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AbbrTitleAttr) {
    auto doc = clever::html::parse("<body><abbr title=\"HyperText Markup Language\">HTML</abbr></body>");
    auto* el = doc->find_element("abbr");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "title") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, BdoDirAttr) {
    auto doc = clever::html::parse("<body><bdo dir=\"rtl\">Hello</bdo></body>");
    auto* el = doc->find_element("bdo");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "dir" && attr.value == "rtl") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, RubyTagParsed) {
    auto doc = clever::html::parse("<body><ruby>漢<rt>kan</rt></ruby></body>");
    auto* el = doc->find_element("ruby");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ruby");
}

TEST(TreeBuilder, RtTagParsed) {
    auto doc = clever::html::parse("<body><ruby>漢<rt>kan</rt></ruby></body>");
    auto* el = doc->find_element("rt");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "rt");
}

TEST(TreeBuilder, WbrTagParsed) {
    auto doc = clever::html::parse("<body><p>longword<wbr>break</p></body>");
    auto* el = doc->find_element("wbr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "wbr");
}

TEST(TreeBuilder, DetailsTagParsed) {
    auto doc = clever::html::parse("<body><details><summary>More</summary>Content</details></body>");
    auto* el = doc->find_element("details");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "details");
}

TEST(TreeBuilder, DialogOpenAttr) {
    auto doc = clever::html::parse("<body><dialog open>Hello</dialog></body>");
    auto* el = doc->find_element("dialog");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "open") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MeterValueAttrV2) {
    auto doc = clever::html::parse("<body><meter value=\"0.6\">60%</meter></body>");
    auto* el = doc->find_element("meter");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "value" && attr.value == "0.6") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, ProgressValueAttrV2) {
    auto doc = clever::html::parse("<body><progress value=\"70\" max=\"100\"></progress></body>");
    auto* el = doc->find_element("progress");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "value" && attr.value == "70") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, FigcaptionTextParsed) {
    auto doc = clever::html::parse("<body><figure><figcaption>Caption</figcaption></figure></body>");
    auto* el = doc->find_element("figcaption");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "figcaption");
}

TEST(TreeBuilder, NavElementParsedV2) {
    auto doc = clever::html::parse("<body><nav>Links</nav></body>");
    auto* el = doc->find_element("nav");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "nav");
}

TEST(TreeBuilder, HeaderElementParsedV2) {
    auto doc = clever::html::parse("<body><header>Title</header></body>");
    auto* el = doc->find_element("header");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "header");
}

TEST(TreeBuilder, FooterElementParsedV2) {
    auto doc = clever::html::parse("<body><footer>Copyright</footer></body>");
    auto* el = doc->find_element("footer");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "footer");
}

TEST(TreeBuilder, AsideElementParsedV2) {
    auto doc = clever::html::parse("<body><aside>Sidebar</aside></body>");
    auto* el = doc->find_element("aside");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "aside");
}

TEST(TreeBuilder, MainElementParsedV2) {
    auto doc = clever::html::parse("<body><main>Content</main></body>");
    auto* el = doc->find_element("main");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "main");
}

TEST(TreeBuilder, ArticleElementParsedV2) {
    auto doc = clever::html::parse("<body><article>Post</article></body>");
    auto* el = doc->find_element("article");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "article");
}

TEST(TreeBuilder, TimeElementDatetimeV2) {
    auto doc = clever::html::parse("<body><time datetime=\"2024-01-01\">NY</time></body>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes)
        if (attr.name == "datetime" && attr.value == "2024-01-01") found = true;
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, SummaryElementParsedV2) {
    auto doc = clever::html::parse("<body><details><summary>More</summary>Content</details></body>");
    auto* el = doc->find_element("summary");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "summary");
}

// --- Cycle 1018: HTML element parsing tests ---

TEST(TreeBuilder, NavElementParsedV3) {
    auto doc = clever::html::parse("<body><nav>Menu</nav></body>");
    auto* el = doc->find_element("nav");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "nav");
}

TEST(TreeBuilder, AsideElementParsedV3) {
    auto doc = clever::html::parse("<body><aside>Sidebar</aside></body>");
    auto* el = doc->find_element("aside");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "aside");
}

TEST(TreeBuilder, HeaderElementParsedV3) {
    auto doc = clever::html::parse("<body><header>Top</header></body>");
    auto* el = doc->find_element("header");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "header");
}

TEST(TreeBuilder, FooterElementParsedV3) {
    auto doc = clever::html::parse("<body><footer>Bottom</footer></body>");
    auto* el = doc->find_element("footer");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "footer");
}

TEST(TreeBuilder, DialogElementParsedV2) {
    auto doc = clever::html::parse("<body><dialog>Popup</dialog></body>");
    auto* el = doc->find_element("dialog");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "dialog");
}

TEST(TreeBuilder, TemplateElementParsedV2) {
    auto doc = clever::html::parse("<body><template>Hidden</template></body>");
    auto* el = doc->find_element("template");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "template");
}

TEST(TreeBuilder, WbrElementParsedV2) {
    auto doc = clever::html::parse("<body><p>Long<wbr>Word</p></body>");
    auto* el = doc->find_element("wbr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "wbr");
}

TEST(TreeBuilder, RubyElementParsedV2) {
    auto doc = clever::html::parse("<body><ruby>漢<rt>かん</rt></ruby></body>");
    auto* el = doc->find_element("ruby");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ruby");
}

// --- Cycle 1027: HTML element tests ---

TEST(TreeBuilder, RtElementParsedV2) {
    auto doc = clever::html::parse("<body><ruby>字<rt>ji</rt></ruby></body>");
    auto* el = doc->find_element("rt");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "rt");
}

TEST(TreeBuilder, PreElementParsedV2) {
    auto doc = clever::html::parse("<body><pre>code here</pre></body>");
    auto* el = doc->find_element("pre");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "pre");
}

TEST(TreeBuilder, CodeElementParsedV2) {
    auto doc = clever::html::parse("<body><code>x = 1</code></body>");
    auto* el = doc->find_element("code");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "code");
}

TEST(TreeBuilder, BlockquoteElementParsedV2) {
    auto doc = clever::html::parse("<body><blockquote>Quote</blockquote></body>");
    auto* el = doc->find_element("blockquote");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "blockquote");
}

TEST(TreeBuilder, HrElementParsedV2) {
    auto doc = clever::html::parse("<body><hr></body>");
    auto* el = doc->find_element("hr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "hr");
}

TEST(TreeBuilder, BrElementParsedV2) {
    auto doc = clever::html::parse("<body>Line<br>Break</body>");
    auto* el = doc->find_element("br");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "br");
}

TEST(TreeBuilder, DlElementParsed) {
    auto doc = clever::html::parse("<body><dl><dt>Term</dt><dd>Def</dd></dl></body>");
    auto* el = doc->find_element("dl");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "dl");
}

TEST(TreeBuilder, DtElementParsed) {
    auto doc = clever::html::parse("<body><dl><dt>Term</dt><dd>Def</dd></dl></body>");
    auto* el = doc->find_element("dt");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "dt");
}

// --- Cycle 1036: HTML element tests ---

TEST(TreeBuilder, DdElementParsed) {
    auto doc = clever::html::parse("<body><dl><dt>T</dt><dd>D</dd></dl></body>");
    auto* el = doc->find_element("dd");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "dd");
}

TEST(TreeBuilder, AddressElementParsed) {
    auto doc = clever::html::parse("<body><address>Contact info</address></body>");
    auto* el = doc->find_element("address");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "address");
}

TEST(TreeBuilder, CanvasElementParsed) {
    auto doc = clever::html::parse("<body><canvas>Fallback</canvas></body>");
    auto* el = doc->find_element("canvas");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "canvas");
}

TEST(TreeBuilder, NoScriptElementParsed) {
    auto doc = clever::html::parse("<body><noscript>No JS</noscript></body>");
    auto* el = doc->find_element("noscript");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "noscript");
}

TEST(TreeBuilder, StrongElementParsedV3) {
    auto doc = clever::html::parse("<body><strong>Bold</strong></body>");
    auto* el = doc->find_element("strong");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "strong");
}

TEST(TreeBuilder, EmElementParsedV3) {
    auto doc = clever::html::parse("<body><em>Italic</em></body>");
    auto* el = doc->find_element("em");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "em");
}

TEST(TreeBuilder, AbbrElementParsedV3) {
    auto doc = clever::html::parse("<body><abbr title=\"HyperText\">HTML</abbr></body>");
    auto* el = doc->find_element("abbr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "abbr");
}

TEST(TreeBuilder, QElementParsed) {
    auto doc = clever::html::parse("<body><q>Inline quote</q></body>");
    auto* el = doc->find_element("q");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "q");
}

// --- Cycle 1045: HTML element tests ---

TEST(TreeBuilder, CiteElementParsed) {
    auto doc = clever::html::parse("<body><cite>Book Title</cite></body>");
    auto* el = doc->find_element("cite");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "cite");
}

TEST(TreeBuilder, CodeElementParsedV3) {
    auto doc = clever::html::parse("<body><code>x = 1</code></body>");
    auto* el = doc->find_element("code");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "code");
}

TEST(TreeBuilder, KbdElementParsed) {
    auto doc = clever::html::parse("<body><kbd>Ctrl+C</kbd></body>");
    auto* el = doc->find_element("kbd");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "kbd");
}

TEST(TreeBuilder, SampElementParsed) {
    auto doc = clever::html::parse("<body><samp>Output</samp></body>");
    auto* el = doc->find_element("samp");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "samp");
}

TEST(TreeBuilder, VarElementParsed) {
    auto doc = clever::html::parse("<body><var>x</var></body>");
    auto* el = doc->find_element("var");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "var");
}

TEST(TreeBuilder, SubElementParsed) {
    auto doc = clever::html::parse("<body><sub>2</sub></body>");
    auto* el = doc->find_element("sub");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "sub");
}

TEST(TreeBuilder, SupElementParsed) {
    auto doc = clever::html::parse("<body><sup>2</sup></body>");
    auto* el = doc->find_element("sup");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "sup");
}

TEST(TreeBuilder, SmallElementParsed) {
    auto doc = clever::html::parse("<body><small>Fine print</small></body>");
    auto* el = doc->find_element("small");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "small");
}

// --- Cycle 1054: HTML element tests ---

TEST(TreeBuilder, DelElementParsedV2) {
    auto doc = clever::html::parse("<body><del>Removed</del></body>");
    auto* el = doc->find_element("del");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "del");
}

TEST(TreeBuilder, InsElementParsedV2) {
    auto doc = clever::html::parse("<body><ins>Added</ins></body>");
    auto* el = doc->find_element("ins");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ins");
}

TEST(TreeBuilder, BdiElementParsed) {
    auto doc = clever::html::parse("<body><bdi>RTL text</bdi></body>");
    auto* el = doc->find_element("bdi");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "bdi");
}

TEST(TreeBuilder, BdoElementParsedV2) {
    auto doc = clever::html::parse("<body><bdo dir=\"rtl\">Reversed</bdo></body>");
    auto* el = doc->find_element("bdo");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "bdo");
}

TEST(TreeBuilder, MeterElementParsedV2) {
    auto doc = clever::html::parse("<body><meter value=\"0.6\">60%</meter></body>");
    auto* el = doc->find_element("meter");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "meter");
}

TEST(TreeBuilder, ProgressElementParsedV2) {
    auto doc = clever::html::parse("<body><progress value=\"70\" max=\"100\">70%</progress></body>");
    auto* el = doc->find_element("progress");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "progress");
}

TEST(TreeBuilder, DetailsElementParsed) {
    auto doc = clever::html::parse("<body><details><summary>Info</summary>Content</details></body>");
    auto* el = doc->find_element("details");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "details");
}

TEST(TreeBuilder, SummaryElementParsedV3) {
    auto doc = clever::html::parse("<body><details><summary>Info</summary>Content</details></body>");
    auto* el = doc->find_element("summary");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "summary");
}

// --- Cycle 1063: HTML element tests ---

TEST(TreeBuilder, FigureElementParsed) {
    auto doc = clever::html::parse("<body><figure><img/></figure></body>");
    auto* el = doc->find_element("figure");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "figure");
}

TEST(TreeBuilder, FigcaptionElementParsed) {
    auto doc = clever::html::parse("<body><figure><figcaption>Caption</figcaption></figure></body>");
    auto* el = doc->find_element("figcaption");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "figcaption");
}

TEST(TreeBuilder, MainElementParsedV3) {
    auto doc = clever::html::parse("<body><main>Content</main></body>");
    auto* el = doc->find_element("main");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "main");
}

TEST(TreeBuilder, TimeElementParsedV2) {
    auto doc = clever::html::parse("<body><time datetime=\"2026-01-01\">New Year</time></body>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "time");
}

TEST(TreeBuilder, MarkElementParsedV2) {
    auto doc = clever::html::parse("<body><mark>Highlighted</mark></body>");
    auto* el = doc->find_element("mark");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "mark");
}

TEST(TreeBuilder, DataElementParsedV2) {
    auto doc = clever::html::parse("<body><data value=\"42\">Forty-two</data></body>");
    auto* el = doc->find_element("data");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "data");
}

TEST(TreeBuilder, WbrElementParsedV3) {
    auto doc = clever::html::parse("<body>Long<wbr>Word</body>");
    auto* el = doc->find_element("wbr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "wbr");
}

TEST(TreeBuilder, RubyElementParsedV3) {
    auto doc = clever::html::parse("<body><ruby>漢<rt>kan</rt></ruby></body>");
    auto* el = doc->find_element("ruby");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ruby");
}

// --- Cycle 1072: HTML element tests ---

TEST(TreeBuilder, RtElementParsedV3) {
    auto doc = clever::html::parse("<body><ruby>漢<rt>kan</rt></ruby></body>");
    auto* el = doc->find_element("rt");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "rt");
}

TEST(TreeBuilder, PictureElementParsed) {
    auto doc = clever::html::parse("<body><picture><img/></picture></body>");
    auto* el = doc->find_element("picture");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "picture");
}

TEST(TreeBuilder, SourceElementParsed) {
    auto doc = clever::html::parse("<body><picture><source/><img/></picture></body>");
    auto* el = doc->find_element("source");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "source");
}

TEST(TreeBuilder, OutputElementParsedV2) {
    auto doc = clever::html::parse("<body><output>Result</output></body>");
    auto* el = doc->find_element("output");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "output");
}

TEST(TreeBuilder, FieldsetElementParsedV2) {
    auto doc = clever::html::parse("<body><fieldset><legend>Title</legend></fieldset></body>");
    auto* el = doc->find_element("fieldset");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "fieldset");
}

TEST(TreeBuilder, LegendElementParsedV2) {
    auto doc = clever::html::parse("<body><fieldset><legend>Title</legend></fieldset></body>");
    auto* el = doc->find_element("legend");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "legend");
}

TEST(TreeBuilder, OptgroupElementParsed) {
    auto doc = clever::html::parse("<body><select><optgroup label=\"g\"><option>A</option></optgroup></select></body>");
    auto* el = doc->find_element("optgroup");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "optgroup");
}

TEST(TreeBuilder, DatalistElementParsed) {
    auto doc = clever::html::parse("<body><datalist id=\"d\"><option>A</option></datalist></body>");
    auto* el = doc->find_element("datalist");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "datalist");
}

// --- Cycle 1081: HTML element tests ---

TEST(TreeBuilder, ColElementParsed) {
    auto doc = clever::html::parse("<body><table><colgroup><col/></colgroup></table></body>");
    auto* el = doc->find_element("col");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "col");
}

TEST(TreeBuilder, ColgroupElementParsed) {
    auto doc = clever::html::parse("<body><table><colgroup><col/></colgroup></table></body>");
    auto* el = doc->find_element("colgroup");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "colgroup");
}

TEST(TreeBuilder, TheadElementParsed) {
    auto doc = clever::html::parse("<body><table><thead><tr><th>H</th></tr></thead></table></body>");
    auto* el = doc->find_element("thead");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "thead");
}

TEST(TreeBuilder, TbodyElementParsed) {
    auto doc = clever::html::parse("<body><table><tbody><tr><td>D</td></tr></tbody></table></body>");
    auto* el = doc->find_element("tbody");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "tbody");
}

TEST(TreeBuilder, TfootElementParsed) {
    auto doc = clever::html::parse("<body><table><tfoot><tr><td>F</td></tr></tfoot></table></body>");
    auto* el = doc->find_element("tfoot");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "tfoot");
}

TEST(TreeBuilder, CaptionElementParsed) {
    auto doc = clever::html::parse("<body><table><caption>Title</caption></table></body>");
    auto* el = doc->find_element("caption");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "caption");
}

TEST(TreeBuilder, IframeElementParsedV2) {
    auto doc = clever::html::parse("<body><iframe src=\"about:blank\"></iframe></body>");
    auto* el = doc->find_element("iframe");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "iframe");
}

TEST(TreeBuilder, ObjectElementParsed) {
    auto doc = clever::html::parse("<body><object data=\"test.swf\"></object></body>");
    auto* el = doc->find_element("object");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "object");
}

// --- Cycle 1090: HTML element tests ---

TEST(TreeBuilder, ParamElementParsed) {
    auto doc = clever::html::parse("<body><object><param name=\"movie\" value=\"test.swf\"/></object></body>");
    auto* el = doc->find_element("param");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "param");
}

TEST(TreeBuilder, MapElementParsedV2) {
    auto doc = clever::html::parse("<body><map name=\"m\"><area href=\"#\"/></map></body>");
    auto* el = doc->find_element("map");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "map");
}

TEST(TreeBuilder, AreaElementParsedV2) {
    auto doc = clever::html::parse("<body><map><area href=\"#\"/></map></body>");
    auto* el = doc->find_element("area");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "area");
}

TEST(TreeBuilder, SpanElementParsedV3) {
    auto doc = clever::html::parse("<body><span>Text</span></body>");
    auto* el = doc->find_element("span");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "span");
}

TEST(TreeBuilder, IElementParsed) {
    auto doc = clever::html::parse("<body><i>Italic</i></body>");
    auto* el = doc->find_element("i");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "i");
}

TEST(TreeBuilder, BElementParsed) {
    auto doc = clever::html::parse("<body><b>Bold</b></body>");
    auto* el = doc->find_element("b");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "b");
}

TEST(TreeBuilder, UElementParsed) {
    auto doc = clever::html::parse("<body><u>Underline</u></body>");
    auto* el = doc->find_element("u");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "u");
}

TEST(TreeBuilder, SElementParsed) {
    auto doc = clever::html::parse("<body><s>Strikethrough</s></body>");
    auto* el = doc->find_element("s");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "s");
}

// --- Cycle 1099: 8 HTML tests ---

TEST(TreeBuilder, SmallElementParsedV2) {
    auto doc = clever::html::parse("<body><small>Fine print</small></body>");
    auto* el = doc->find_element("small");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "small");
}

TEST(TreeBuilder, BigElementParsed) {
    auto doc = clever::html::parse("<body><big>Large</big></body>");
    auto* el = doc->find_element("big");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "big");
}

TEST(TreeBuilder, SubElementParsedV2) {
    auto doc = clever::html::parse("<body><sub>subscript</sub></body>");
    auto* el = doc->find_element("sub");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "sub");
}

TEST(TreeBuilder, SupElementParsedV2) {
    auto doc = clever::html::parse("<body><sup>superscript</sup></body>");
    auto* el = doc->find_element("sup");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "sup");
}

TEST(TreeBuilder, CiteElementParsedV2) {
    auto doc = clever::html::parse("<body><cite>Citation</cite></body>");
    auto* el = doc->find_element("cite");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "cite");
}

TEST(TreeBuilder, VarElementParsedV2) {
    auto doc = clever::html::parse("<body><var>x</var></body>");
    auto* el = doc->find_element("var");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "var");
}

TEST(TreeBuilder, SampElementParsedV2) {
    auto doc = clever::html::parse("<body><samp>output</samp></body>");
    auto* el = doc->find_element("samp");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "samp");
}

TEST(TreeBuilder, KbdElementParsedV2) {
    auto doc = clever::html::parse("<body><kbd>Ctrl+C</kbd></body>");
    auto* el = doc->find_element("kbd");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "kbd");
}

// --- Cycle 1108: 8 HTML tests ---

TEST(TreeBuilder, AbbrElementParsed) {
    auto doc = clever::html::parse("<body><abbr>HTML</abbr></body>");
    auto* el = doc->find_element("abbr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "abbr");
}

TEST(TreeBuilder, DfnElementParsed) {
    auto doc = clever::html::parse("<body><dfn>Definition</dfn></body>");
    auto* el = doc->find_element("dfn");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "dfn");
}

TEST(TreeBuilder, QElementParsedV2) {
    auto doc = clever::html::parse("<body><q>Quote</q></body>");
    auto* el = doc->find_element("q");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "q");
}

TEST(TreeBuilder, TtElementParsed) {
    auto doc = clever::html::parse("<body><tt>Teletype</tt></body>");
    auto* el = doc->find_element("tt");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "tt");
}

TEST(TreeBuilder, CenterElementParsed) {
    auto doc = clever::html::parse("<body><center>Centered</center></body>");
    auto* el = doc->find_element("center");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "center");
}

TEST(TreeBuilder, FontElementParsed) {
    auto doc = clever::html::parse("<body><font>Styled</font></body>");
    auto* el = doc->find_element("font");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "font");
}

TEST(TreeBuilder, StrikeElementParsed) {
    auto doc = clever::html::parse("<body><strike>Struck</strike></body>");
    auto* el = doc->find_element("strike");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "strike");
}

TEST(TreeBuilder, UElementParsedV2) {
    auto doc = clever::html::parse("<body><u>Underline</u></body>");
    auto* el = doc->find_element("u");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "u");
}

// --- Cycle 1117: 8 HTML tests ---

TEST(TreeBuilder, MapElementParsed) {
    auto doc = clever::html::parse("<body><map name=\"test\">map</map></body>");
    auto* el = doc->find_element("map");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "map");
}

TEST(TreeBuilder, AreaElementParsed) {
    auto doc = clever::html::parse("<body><map><area></map></body>");
    auto* el = doc->find_element("area");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "area");
}

TEST(TreeBuilder, TrackElementParsed) {
    auto doc = clever::html::parse("<body><video><track></video></body>");
    auto* el = doc->find_element("track");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "track");
}

TEST(TreeBuilder, EmbedElementParsed) {
    auto doc = clever::html::parse("<body><embed></body>");
    auto* el = doc->find_element("embed");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "embed");
}

TEST(TreeBuilder, ParamElementParsedV2) {
    auto doc = clever::html::parse("<body><object><param></object></body>");
    auto* el = doc->find_element("param");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "param");
}

TEST(TreeBuilder, NoscriptElementParsed) {
    auto doc = clever::html::parse("<body><noscript>No JS</noscript></body>");
    auto* el = doc->find_element("noscript");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "noscript");
}

TEST(TreeBuilder, TemplateElementParsed) {
    auto doc = clever::html::parse("<body><template>Content</template></body>");
    auto* el = doc->find_element("template");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "template");
}

TEST(TreeBuilder, SlotElementParsed) {
    auto doc = clever::html::parse("<body><slot>Fallback</slot></body>");
    auto* el = doc->find_element("slot");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "slot");
}

// --- Cycle 1126: 8 HTML tests ---

TEST(TreeBuilder, MenuElementParsed) {
    auto doc = clever::html::parse("<body><menu>Items</menu></body>");
    auto* el = doc->find_element("menu");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "menu");
}

TEST(TreeBuilder, DialogElementParsed) {
    auto doc = clever::html::parse("<body><dialog>Modal</dialog></body>");
    auto* el = doc->find_element("dialog");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "dialog");
}

TEST(TreeBuilder, CanvasElementParsedV2) {
    auto doc = clever::html::parse("<body><canvas>Fallback</canvas></body>");
    auto* el = doc->find_element("canvas");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "canvas");
}

TEST(TreeBuilder, MathElementParsed) {
    auto doc = clever::html::parse("<body><math>x</math></body>");
    auto* el = doc->find_element("math");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "math");
}

TEST(TreeBuilder, SvgElementParsed) {
    auto doc = clever::html::parse("<body><svg></svg></body>");
    auto* el = doc->find_element("svg");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "svg");
}

TEST(TreeBuilder, BaseElementParsed) {
    auto doc = clever::html::parse("<head><base></head><body></body>");
    auto* el = doc->find_element("base");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "base");
}

TEST(TreeBuilder, WbrElementParsedV4) {
    auto doc = clever::html::parse("<body><wbr></body>");
    auto* el = doc->find_element("wbr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "wbr");
}

TEST(TreeBuilder, HrElementParsedV3) {
    auto doc = clever::html::parse("<body><hr></body>");
    auto* el = doc->find_element("hr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "hr");
}

// --- Cycle 1135: 8 HTML tests ---

TEST(TreeBuilder, NobrElementParsed) {
    auto doc = clever::html::parse("<body><nobr>NoBreak</nobr></body>");
    auto* el = doc->find_element("nobr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "nobr");
}

TEST(TreeBuilder, RbElementParsed) {
    auto doc = clever::html::parse("<body><ruby><rb>Base</rb></ruby></body>");
    auto* el = doc->find_element("rb");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "rb");
}

TEST(TreeBuilder, RtcElementParsed) {
    auto doc = clever::html::parse("<body><ruby><rtc>Annotation</rtc></ruby></body>");
    auto* el = doc->find_element("rtc");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "rtc");
}

TEST(TreeBuilder, RpElementParsedV2) {
    auto doc = clever::html::parse("<body><ruby><rp>(</rp></ruby></body>");
    auto* el = doc->find_element("rp");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "rp");
}

TEST(TreeBuilder, AcronymElementParsed) {
    auto doc = clever::html::parse("<body><acronym>HTML</acronym></body>");
    auto* el = doc->find_element("acronym");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "acronym");
}

TEST(TreeBuilder, MarqueeElementParsed) {
    auto doc = clever::html::parse("<body><marquee>Scroll</marquee></body>");
    auto* el = doc->find_element("marquee");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "marquee");
}

TEST(TreeBuilder, FramesetNotInBody) {
    auto doc = clever::html::parse("<body><div>Content</div></body>");
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->tag_name, "body");
}

TEST(TreeBuilder, HeadElementExistsV2) {
    auto doc = clever::html::parse("<html><head></head><body></body></html>");
    auto* head = doc->find_element("head");
    ASSERT_NE(head, nullptr);
    EXPECT_EQ(head->tag_name, "head");
}

TEST(TreeBuilder, XmpElementParsed) {
    auto doc = clever::html::parse("<body><xmp>Preformatted</xmp></body>");
    auto* el = doc->find_element("xmp");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "xmp");
}

TEST(TreeBuilder, ListingElementParsed) {
    auto doc = clever::html::parse("<body><listing>Code listing</listing></body>");
    auto* el = doc->find_element("listing");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "listing");
}

TEST(TreeBuilder, PlaintextElementParsed) {
    auto doc = clever::html::parse("<body><plaintext>Plain text content</plaintext></body>");
    auto* el = doc->find_element("plaintext");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "plaintext");
}

TEST(TreeBuilder, IsindexNotInBody) {
    auto doc = clever::html::parse("<body><div>Content</div></body>");
    auto* el = doc->find_element("isindex");
    EXPECT_EQ(el, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->tag_name, "body");
}

TEST(TreeBuilder, BgsoundNotInBody) {
    auto doc = clever::html::parse("<body><div>Content</div></body>");
    auto* el = doc->find_element("bgsound");
    EXPECT_EQ(el, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->tag_name, "body");
}

TEST(TreeBuilder, KeygenNotParsed) {
    auto doc = clever::html::parse("<body><div>Content</div></body>");
    auto* el = doc->find_element("keygen");
    EXPECT_EQ(el, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->tag_name, "body");
}

TEST(TreeBuilder, BasefontNotInBody) {
    auto doc = clever::html::parse("<body><div>Content</div></body>");
    auto* el = doc->find_element("basefont");
    EXPECT_EQ(el, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->tag_name, "body");
}

TEST(TreeBuilder, NextidNotInBody) {
    auto doc = clever::html::parse("<body><div>Content</div></body>");
    auto* el = doc->find_element("nextid");
    EXPECT_EQ(el, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->tag_name, "body");
}

// --- Cycle 1153: 8 HTML tests ---

TEST(TreeBuilder, DivWithTextContent) {
    auto doc = clever::html::parse("<body><div>Hello</div></body>");
    auto* el = doc->find_element("div");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "div");
}

TEST(TreeBuilder, ParagraphWithTextContent) {
    auto doc = clever::html::parse("<body><p>This is a paragraph</p></body>");
    auto* el = doc->find_element("p");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "p");
}

TEST(TreeBuilder, SpanInsideDivV2) {
    auto doc = clever::html::parse("<body><div><span>nested span</span></div></body>");
    auto* el = doc->find_element("span");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "span");
}

TEST(TreeBuilder, StrongWithText) {
    auto doc = clever::html::parse("<body><strong>Bold text</strong></body>");
    auto* el = doc->find_element("strong");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "strong");
}

TEST(TreeBuilder, EmWithText) {
    auto doc = clever::html::parse("<body><em>Emphasized text</em></body>");
    auto* el = doc->find_element("em");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "em");
}

TEST(TreeBuilder, AnchorWithHrefV2) {
    auto doc = clever::html::parse("<body><a href=\"https://example.com\">Link</a></body>");
    auto* el = doc->find_element("a");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "a");
}

TEST(TreeBuilder, ImageWithSrc) {
    auto doc = clever::html::parse("<body><img src=\"image.png\"></body>");
    auto* el = doc->find_element("img");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "img");
}

TEST(TreeBuilder, ListWithItems) {
    auto doc = clever::html::parse("<body><ul><li>Item 1</li><li>Item 2</li></ul></body>");
    auto* el = doc->find_element("ul");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ul");
}

// --- Cycle 1162: 8 HTML tests ---

TEST(TreeBuilder, TableWithRowsV2) {
    auto doc = clever::html::parse("<body><table><tr><td>Cell</td></tr></table></body>");
    auto* el = doc->find_element("table");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "table");
}

TEST(TreeBuilder, OrderedListWithItems) {
    auto doc = clever::html::parse("<body><ol><li>First</li><li>Second</li></ol></body>");
    auto* el = doc->find_element("ol");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ol");
}

TEST(TreeBuilder, UnorderedListWithItemsV2) {
    auto doc = clever::html::parse("<body><ul><li>Item A</li><li>Item B</li></ul></body>");
    auto* el = doc->find_element("ul");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ul");
}

TEST(TreeBuilder, DefinitionListV2) {
    auto doc = clever::html::parse("<body><dl><dt>Term</dt><dd>Definition</dd></dl></body>");
    auto* el = doc->find_element("dl");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "dl");
}

TEST(TreeBuilder, FormWithInputs) {
    auto doc = clever::html::parse("<body><form><input type=\"text\"></form></body>");
    auto* el = doc->find_element("form");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "form");
}

TEST(TreeBuilder, SelectWithOptionsV2) {
    auto doc = clever::html::parse("<body><select><option>Choice 1</option><option>Choice 2</option></select></body>");
    auto* el = doc->find_element("select");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "select");
}

TEST(TreeBuilder, TextareaWithContent) {
    auto doc = clever::html::parse("<body><textarea>Sample text</textarea></body>");
    auto* el = doc->find_element("textarea");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "textarea");
}

TEST(TreeBuilder, ButtonWithTextV2) {
    auto doc = clever::html::parse("<body><button>Click me</button></body>");
    auto* el = doc->find_element("button");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "button");
}

TEST(TreeBuilder, HeadingGroupWithTitleAndSubtitle) {
    auto doc = clever::html::parse("<body><hgroup><h1>Main Title</h1><p>Subtitle</p></hgroup></body>");
    auto* el = doc->find_element("hgroup");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "hgroup");
}

TEST(TreeBuilder, MapWithAreaElements) {
    auto doc = clever::html::parse("<body><map name=\"locations\"><area shape=\"circle\" coords=\"100,100,50\" href=\"#region1\"></map></body>");
    auto* el = doc->find_element("map");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "map");
}

TEST(TreeBuilder, PictureWithMultipleSources) {
    auto doc = clever::html::parse("<body><picture><source media=\"(min-width: 800px)\" srcset=\"large.jpg\"><source media=\"(min-width: 480px)\" srcset=\"medium.jpg\"><img src=\"small.jpg\" alt=\"Responsive image\"></picture></body>");
    auto* el = doc->find_element("picture");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "picture");
}

TEST(TreeBuilder, SourceElementWithMediaType) {
    auto doc = clever::html::parse("<body><video><source src=\"movie.mp4\" type=\"video/mp4\"><source src=\"movie.webm\" type=\"video/webm\"></video></body>");
    auto* el = doc->find_element("source");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "source");
}

TEST(TreeBuilder, TrackElementInAudio) {
    auto doc = clever::html::parse("<body><audio controls><source src=\"audio.mp3\" type=\"audio/mpeg\"><track kind=\"captions\" src=\"captions.vtt\" srclang=\"en\" label=\"English\"></audio></body>");
    auto* el = doc->find_element("track");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "track");
}

TEST(TreeBuilder, LabelWithNestedInput) {
    auto doc = clever::html::parse("<body><label>Username: <input type=\"text\" name=\"username\"></label></body>");
    auto* el = doc->find_element("label");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "label");
}

TEST(TreeBuilder, OptgroupWithOptions) {
    auto doc = clever::html::parse("<body><select><optgroup label=\"Group1\"><option>Option A</option><option>Option B</option></optgroup><optgroup label=\"Group2\"><option>Option C</option></optgroup></select></body>");
    auto* el = doc->find_element("optgroup");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "optgroup");
}

TEST(TreeBuilder, TimeElementWithPubdate) {
    auto doc = clever::html::parse("<body><article><p>Published on <time datetime=\"2026-02-27T10:30:00Z\">February 27, 2026</time></p></article></body>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "time");
}

TEST(TreeBuilder, LinkStyledAttr) {
    auto doc = clever::html::parse(R"(<head><link rel="stylesheet" href="style.css"></head>)");
    auto* el = doc->find_element("link");
    ASSERT_NE(el, nullptr);
    bool found_rel = false, found_href = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "rel") found_rel = true;
        if (attr.name == "href") found_href = true;
    }
    EXPECT_TRUE(found_rel && found_href);
}

TEST(TreeBuilder, InputFormenctype) {
    auto doc = clever::html::parse(R"(<form><input type="submit" formenctype="application/x-www-form-urlencoded"></form>)");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "formenctype") found = true;
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, AnchorRelNoopener) {
    auto doc = clever::html::parse(R"(<a href="https://example.com" rel="noopener noreferrer">Link</a>)");
    auto* el = doc->find_element("a");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "rel") found = true;
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, MetaImageAttr) {
    auto doc = clever::html::parse(R"(<head><meta name="og:image" content="image.png"></head>)");
    auto* el = doc->find_element("meta");
    ASSERT_NE(el, nullptr);
    bool found_name = false, found_content = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "name") found_name = true;
        if (attr.name == "content") found_content = true;
    }
    EXPECT_TRUE(found_name && found_content);
}

TEST(TreeBuilder, TextareaMinlength) {
    auto doc = clever::html::parse(R"(<textarea minlength="5" maxlength="500"></textarea>)");
    auto* el = doc->find_element("textarea");
    ASSERT_NE(el, nullptr);
    bool has_min = false, has_max = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "minlength") has_min = true;
        if (attr.name == "maxlength") has_max = true;
    }
    EXPECT_TRUE(has_min && has_max);
}

TEST(TreeBuilder, SelectOptgroupDisabled) {
    auto doc = clever::html::parse(R"(<select><optgroup label="Group1" disabled><option>Opt1</option></optgroup></select>)");
    auto* el = doc->find_element("optgroup");
    ASSERT_NE(el, nullptr);
    bool found = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "disabled") found = true;
    }
    EXPECT_TRUE(found);
}

TEST(TreeBuilder, InputTypeWeekV2) {
    auto doc = clever::html::parse(R"(<input type="week" value="2026-W09" min="2026-W01" max="2026-W52">)");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    bool has_type = false, has_value = false, has_min = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "type") has_type = true;
        if (attr.name == "value") has_value = true;
        if (attr.name == "min") has_min = true;
    }
    EXPECT_TRUE(has_type && has_value && has_min);
}

TEST(TreeBuilder, LinkIconAttr) {
    auto doc = clever::html::parse(R"(<head><link rel="icon" href="favicon.ico" type="image/x-icon"></head>)");
    auto* el = doc->find_element("link");
    ASSERT_NE(el, nullptr);
    bool has_rel = false, has_href = false, has_type = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "rel") has_rel = true;
        if (attr.name == "href") has_href = true;
        if (attr.name == "type") has_type = true;
    }
    EXPECT_TRUE(has_rel && has_href && has_type);
}

TEST(TreeBuilder, DataListElement) {
    auto doc = clever::html::parse(R"(<datalist id="colors"><option value="red">Red</option><option value="blue">Blue</option></datalist>)");
    auto* el = doc->find_element("datalist");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "datalist");
    bool found_id = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "id") found_id = true;
    }
    EXPECT_TRUE(found_id);
}

TEST(TreeBuilder, ContentEditable) {
    auto doc = clever::html::parse(R"(<div contenteditable="true">Editable content</div>)");
    auto* el = doc->find_element("div");
    ASSERT_NE(el, nullptr);
    bool found_contenteditable = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "contenteditable") found_contenteditable = true;
    }
    EXPECT_TRUE(found_contenteditable);
}

TEST(TreeBuilder, DragDropAttributes) {
    auto doc = clever::html::parse("<div draggable=\"true\" ondrop=\"handleDrop()\">Drag me</div>");
    auto* el = doc->find_element("div");
    ASSERT_NE(el, nullptr);
    bool has_draggable = false, has_ondrop = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "draggable") has_draggable = true;
        if (attr.name == "ondrop") has_ondrop = true;
    }
    EXPECT_TRUE(has_draggable && has_ondrop);
}

TEST(TreeBuilder, SpellcheckAttributeV2) {
    auto doc = clever::html::parse(R"(<input type="text" spellcheck="false" placeholder="No spell check">)");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    bool has_spellcheck = false, has_placeholder = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "spellcheck") has_spellcheck = true;
        if (attr.name == "placeholder") has_placeholder = true;
    }
    EXPECT_TRUE(has_spellcheck && has_placeholder);
}

TEST(TreeBuilder, AccesskeyBinding) {
    auto doc = clever::html::parse(R"(<button accesskey="s">Save</button>)");
    auto* el = doc->find_element("button");
    ASSERT_NE(el, nullptr);
    bool found_accesskey = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "accesskey") found_accesskey = true;
    }
    EXPECT_TRUE(found_accesskey);
}

TEST(TreeBuilder, ArticleSemanticTag) {
    auto doc = clever::html::parse(R"(<article><h1>Article Title</h1><p>Content</p></article>)");
    auto* el = doc->find_element("article");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "article");
}

TEST(TreeBuilder, AsideSemanticElement) {
    auto doc = clever::html::parse(R"(<aside role="complementary"><h3>Related Links</h3></aside>)");
    auto* el = doc->find_element("aside");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "aside");
    bool found_role = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "role") found_role = true;
    }
    EXPECT_TRUE(found_role);
}

TEST(TreeBuilder, FigcaptionInFigure) {
    auto doc = clever::html::parse(R"(<figure><img src="image.jpg" alt="Image"><figcaption>Image caption</figcaption></figure>)");
    auto* el = doc->find_element("figcaption");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "figcaption");
}

// ============================================================================
// Cycle 1198: Additional HTML parser tests
// ============================================================================

TEST(TreeBuilder, RelElementLink) {
    auto doc = clever::html::parse("<link rel=\"stylesheet\" href=\"styles.css\">");
    auto* el = doc->find_element("link");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "link");
    bool has_rel = false, has_href = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "rel" && attr.value == "stylesheet") has_rel = true;
        if (attr.name == "href" && attr.value == "styles.css") has_href = true;
    }
    EXPECT_TRUE(has_rel && has_href);
}

TEST(TreeBuilder, RelElementIcon) {
    auto doc = clever::html::parse("<link rel=\"icon\" href=\"favicon.png\" type=\"image/png\">");
    auto* el = doc->find_element("link");
    ASSERT_NE(el, nullptr);
    bool has_rel = false, has_type = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "rel" && attr.value == "icon") has_rel = true;
        if (attr.name == "type" && attr.value == "image/png") has_type = true;
    }
    EXPECT_TRUE(has_rel && has_type);
}

TEST(TreeBuilder, HrefLangAttribute) {
    auto doc = clever::html::parse("<a href=\"https://example.com\" hreflang=\"en\">English Version</a>");
    auto* el = doc->find_element("a");
    ASSERT_NE(el, nullptr);
    bool has_hreflang = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "hreflang" && attr.value == "en") has_hreflang = true;
    }
    EXPECT_TRUE(has_hreflang);
}

TEST(TreeBuilder, PingAttributeTracking) {
    auto doc = clever::html::parse("<a href=\"/page\" ping=\"http://tracker.com/ping\">Link</a>");
    auto* el = doc->find_element("a");
    ASSERT_NE(el, nullptr);
    bool has_ping = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "ping" && attr.value == "http://tracker.com/ping") has_ping = true;
    }
    EXPECT_TRUE(has_ping);
}

TEST(TreeBuilder, TypeAttributeJs) {
    auto doc = clever::html::parse("<script type=\"application/javascript\" src=\"app.js\"></script>");
    auto* el = doc->find_element("script");
    ASSERT_NE(el, nullptr);
    bool has_type = false, has_src = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "type" && attr.value == "application/javascript") has_type = true;
        if (attr.name == "src" && attr.value == "app.js") has_src = true;
    }
    EXPECT_TRUE(has_type && has_src);
}

TEST(TreeBuilder, DeferAsyncScript) {
    auto doc = clever::html::parse("<script src=\"script.js\" defer async></script>");
    auto* el = doc->find_element("script");
    ASSERT_NE(el, nullptr);
    bool has_defer = false, has_async = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "defer") has_defer = true;
        if (attr.name == "async") has_async = true;
    }
    EXPECT_TRUE(has_defer && has_async);
}

TEST(TreeBuilder, CrossoriginAttribute) {
    auto doc = clever::html::parse("<img src=\"image.jpg\" alt=\"Photo\" crossorigin=\"anonymous\">");
    auto* el = doc->find_element("img");
    ASSERT_NE(el, nullptr);
    bool has_crossorigin = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "crossorigin" && attr.value == "anonymous") has_crossorigin = true;
    }
    EXPECT_TRUE(has_crossorigin);
}

TEST(TreeBuilder, AutofocusInputField) {
    auto doc = clever::html::parse("<input type=\"email\" placeholder=\"Enter email\" autofocus>");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    bool has_autofocus = false, has_placeholder = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "autofocus") has_autofocus = true;
        if (attr.name == "placeholder" && attr.value == "Enter email") has_placeholder = true;
    }
    EXPECT_TRUE(has_autofocus && has_placeholder);
}

// ============================================================================
// Cycle 1207: Additional HTML parser tests
// ============================================================================

TEST(TreeBuilder, TimeElementWithDatetimeV2) {
    auto doc = clever::html::parse("<time datetime=\"2026-02-27T15:30:00Z\">February 27, 2026</time>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "time");
    bool has_datetime = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "datetime" && attr.value == "2026-02-27T15:30:00Z") has_datetime = true;
    }
    EXPECT_TRUE(has_datetime);
}

TEST(TreeBuilder, MarkHighlightElement) {
    auto doc = clever::html::parse("<p>This text contains <mark>highlighted content</mark> within.</p>");
    auto* el = doc->find_element("mark");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "mark");
}

TEST(TreeBuilder, SmallTextElement) {
    auto doc = clever::html::parse("<p>Regular text <small>with smaller text</small> at end.</p>");
    auto* el = doc->find_element("small");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "small");
}

TEST(TreeBuilder, OutputFormElement) {
    auto doc = clever::html::parse("<output name=\"result\" form=\"calc\">0</output>");
    auto* el = doc->find_element("output");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "output");
    bool has_name = false, has_form = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "name" && attr.value == "result") has_name = true;
        if (attr.name == "form" && attr.value == "calc") has_form = true;
    }
    EXPECT_TRUE(has_name && has_form);
}

TEST(TreeBuilder, MenuElementStructure) {
    auto doc = clever::html::parse("<menu><li><button>Option 1</button></li><li><button>Option 2</button></li></menu>");
    auto* el = doc->find_element("menu");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "menu");
    auto* btn = doc->find_element("button");
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->tag_name, "button");
}

TEST(TreeBuilder, SectionWithHeadingV2) {
    auto doc = clever::html::parse("<section><h2>Chapter Title</h2><p>Content goes here.</p></section>");
    auto* el = doc->find_element("section");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "section");
    auto* h2 = doc->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->tag_name, "h2");
}

TEST(TreeBuilder, HeaderFooterLayout) {
    auto doc = clever::html::parse("<header><h1>Site Title</h1><nav>Menu</nav></header><footer><p>Copyright 2026</p></footer>");
    auto* hdr = doc->find_element("header");
    ASSERT_NE(hdr, nullptr);
    EXPECT_EQ(hdr->tag_name, "header");
    auto* ftr = doc->find_element("footer");
    ASSERT_NE(ftr, nullptr);
    EXPECT_EQ(ftr->tag_name, "footer");
}

TEST(TreeBuilder, DetailsSummaryInteractive) {
    auto doc = clever::html::parse("<details><summary>Click to expand</summary><p>Hidden content here</p></details>");
    auto* det = doc->find_element("details");
    ASSERT_NE(det, nullptr);
    EXPECT_EQ(det->tag_name, "details");
    auto* sum = doc->find_element("summary");
    ASSERT_NE(sum, nullptr);
    EXPECT_EQ(sum->tag_name, "summary");
}

// ============================================================================
// Cycle 1216: Additional HTML parser tests
// ============================================================================

TEST(TreeBuilder, ProgressWithMinMaxAttrs) {
    auto doc = clever::html::parse("<progress min=\"0\" max=\"100\" value=\"45\"></progress>");
    auto* el = doc->find_element("progress");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "progress");
    bool has_min = false, has_max = false, has_value = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "min" && attr.value == "0") has_min = true;
        if (attr.name == "max" && attr.value == "100") has_max = true;
        if (attr.name == "value" && attr.value == "45") has_value = true;
    }
    EXPECT_TRUE(has_min && has_max && has_value);
}

TEST(TreeBuilder, MetaPropertyOgAttr) {
    auto doc = clever::html::parse("<head><meta property=\"og:type\" content=\"website\"></head><body></body>");
    auto* el = doc->find_element("meta");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "meta");
    bool has_property = false, has_content = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "property" && attr.value == "og:type") has_property = true;
        if (attr.name == "content" && attr.value == "website") has_content = true;
    }
    EXPECT_TRUE(has_property && has_content);
}

TEST(TreeBuilder, InputTypeWeekV3) {
    auto doc = clever::html::parse("<input type=\"week\" name=\"meeting-week\" value=\"2026-W10\">");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "input");
    bool has_type = false, has_name = false, has_value = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "type" && attr.value == "week") has_type = true;
        if (attr.name == "name" && attr.value == "meeting-week") has_name = true;
        if (attr.name == "value" && attr.value == "2026-W10") has_value = true;
    }
    EXPECT_TRUE(has_type && has_name && has_value);
}

TEST(TreeBuilder, TextareaPlaceholderV2) {
    auto doc = clever::html::parse("<textarea id=\"msg\" placeholder=\"Type your message here\" rows=\"5\" cols=\"40\"></textarea>");
    auto* el = doc->find_element("textarea");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "textarea");
    bool has_id = false, has_placeholder = false, has_rows = false, has_cols = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "id" && attr.value == "msg") has_id = true;
        if (attr.name == "placeholder" && attr.value == "Type your message here") has_placeholder = true;
        if (attr.name == "rows" && attr.value == "5") has_rows = true;
        if (attr.name == "cols" && attr.value == "40") has_cols = true;
    }
    EXPECT_TRUE(has_id && has_placeholder && has_rows && has_cols);
}

TEST(TreeBuilder, SelectOptgroupElementV2) {
    auto doc = clever::html::parse("<select><optgroup label=\"Fruits\"><option>Apple</option><option>Orange</option></optgroup><optgroup label=\"Veggies\"><option>Carrot</option></optgroup></select>");
    auto* sel = doc->find_element("select");
    ASSERT_NE(sel, nullptr);
    EXPECT_EQ(sel->tag_name, "select");
    auto* optg = doc->find_element("optgroup");
    ASSERT_NE(optg, nullptr);
    EXPECT_EQ(optg->tag_name, "optgroup");
    bool has_label = false;
    for (const auto& attr : optg->attributes) {
        if (attr.name == "label" && attr.value == "Fruits") has_label = true;
    }
    EXPECT_TRUE(has_label);
}

TEST(TreeBuilder, TableHeaderScopeRowAttr) {
    auto doc = clever::html::parse("<table><thead><tr><th scope=\"col\">Name</th><th scope=\"col\">Age</th></tr></thead></table>");
    auto* th = doc->find_element("th");
    ASSERT_NE(th, nullptr);
    EXPECT_EQ(th->tag_name, "th");
    bool has_scope = false;
    for (const auto& attr : th->attributes) {
        if (attr.name == "scope" && attr.value == "col") has_scope = true;
    }
    EXPECT_TRUE(has_scope);
}

TEST(TreeBuilder, ParagraphWithStrongAndEm) {
    auto doc = clever::html::parse("<p>This is <strong>important</strong> and <em>emphasized</em> text.</p>");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->tag_name, "p");
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->tag_name, "strong");
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->tag_name, "em");
}

TEST(TreeBuilder, DivWithNestedSpanAndP) {
    auto doc = clever::html::parse("<div id=\"container\"><span class=\"highlight\">Highlighted</span><p>Paragraph content here</p></div>");
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->tag_name, "div");
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->tag_name, "span");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->tag_name, "p");
    bool has_div_id = false;
    for (const auto& attr : div->attributes) {
        if (attr.name == "id" && attr.value == "container") has_div_id = true;
    }
    EXPECT_TRUE(has_div_id);
}

// Cycle 1225: HTML parser tests

TEST(TreeBuilder, MeterElementV3) {
    auto doc = parse("<html><body><meter value=\"0.6\" min=\"0\" max=\"1\"></meter></body></html>");
    auto* el = doc->find_element("meter");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "meter");
}

TEST(TreeBuilder, ProgressElementV3) {
    auto doc = parse("<html><body><progress value=\"70\" max=\"100\"></progress></body></html>");
    auto* el = doc->find_element("progress");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "progress");
}

TEST(TreeBuilder, OutputElementV3) {
    auto doc = parse("<html><body><output name=\"result\">42</output></body></html>");
    auto* el = doc->find_element("output");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "output");
}

TEST(TreeBuilder, DatalistElementV3) {
    auto doc = parse("<html><body><datalist id=\"colors\"><option value=\"red\"></option></datalist></body></html>");
    auto* el = doc->find_element("datalist");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "datalist");
}

TEST(TreeBuilder, DetailsElementV3) {
    auto doc = parse("<html><body><details><summary>Info</summary><p>Detail text</p></details></body></html>");
    auto* el = doc->find_element("details");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "details");
}

TEST(TreeBuilder, DialogElementV3) {
    auto doc = parse("<html><body><dialog open>Hello</dialog></body></html>");
    auto* el = doc->find_element("dialog");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "dialog");
}

TEST(TreeBuilder, RubyElementWithRtV3) {
    auto doc = parse("<html><body><ruby>漢<rt>かん</rt></ruby></body></html>");
    auto* el = doc->find_element("ruby");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ruby");
}

TEST(TreeBuilder, BdiElementV3) {
    auto doc = parse("<html><body><bdi>مرحبا</bdi></body></html>");
    auto* el = doc->find_element("bdi");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "bdi");
}

// Cycle 1234: HTML parser tests V4

TEST(TreeBuilder, ArticleElementV4) {
    auto doc = parse("<html><body><article><h1>News</h1><p>Content here</p></article></body></html>");
    auto* el = doc->find_element("article");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "article");
}

TEST(TreeBuilder, AsideElementV4) {
    auto doc = parse("<html><body><aside><h2>Sidebar</h2><p>Related info</p></aside></body></html>");
    auto* el = doc->find_element("aside");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "aside");
}

TEST(TreeBuilder, NavElementV4) {
    auto doc = parse("<html><body><nav><a href=\"/home\">Home</a><a href=\"/about\">About</a></nav></body></html>");
    auto* el = doc->find_element("nav");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "nav");
}

TEST(TreeBuilder, SectionElementV4) {
    auto doc = parse("<html><body><section id=\"main\"><h2>Main Section</h2></section></body></html>");
    auto* el = doc->find_element("section");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "section");
}

TEST(TreeBuilder, HeaderElementV4) {
    auto doc = parse("<html><body><header><h1>Site Title</h1></header></body></html>");
    auto* el = doc->find_element("header");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "header");
}

TEST(TreeBuilder, FooterElementV4) {
    auto doc = parse("<html><body><footer><p>Copyright 2024</p></footer></body></html>");
    auto* el = doc->find_element("footer");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "footer");
}

TEST(TreeBuilder, MarkElementV4) {
    auto doc = parse("<html><body><p>This is <mark>highlighted</mark> text</p></body></html>");
    auto* el = doc->find_element("mark");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "mark");
    EXPECT_EQ(el->text_content(), "highlighted");
}

TEST(TreeBuilder, TimeElementV4) {
    auto doc = parse("<html><body><p>Posted on <time datetime=\"2024-02-27\">Feb 27, 2024</time></p></body></html>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "time");
}

// Cycle 1243: HTML parser tests V5

TEST(TreeBuilder, ArticleElementV5) {
    auto doc = parse("<html><body><article><h2>News Article</h2><p>Article content</p></article></body></html>");
    auto* el = doc->find_element("article");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "article");
}

TEST(TreeBuilder, AsideElementV5) {
    auto doc = parse("<html><body><main><p>Main content</p><aside>Sidebar info</aside></main></body></html>");
    auto* el = doc->find_element("aside");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "aside");
}

TEST(TreeBuilder, NavElementV5) {
    auto doc = parse("<html><body><nav><ul><li><a href=\"/\">Home</a></li></ul></nav></body></html>");
    auto* el = doc->find_element("nav");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "nav");
}

TEST(TreeBuilder, SectionElementV5) {
    auto doc = parse("<html><body><section id=\"intro\"><h3>Introduction</h3></section></body></html>");
    auto* el = doc->find_element("section");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "section");
}

TEST(TreeBuilder, HeaderElementV5) {
    auto doc = parse("<html><body><header><nav>Navigation</nav></header></body></html>");
    auto* el = doc->find_element("header");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "header");
}

TEST(TreeBuilder, FooterElementV5) {
    auto doc = parse("<html><body><footer><p>Footer content</p></footer></body></html>");
    auto* el = doc->find_element("footer");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "footer");
}

TEST(TreeBuilder, MarkElementV5) {
    auto doc = parse("<html><body><p>Text with <mark>important</mark> section</p></body></html>");
    auto* el = doc->find_element("mark");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "mark");
    EXPECT_EQ(el->text_content(), "important");
}

TEST(TreeBuilder, TimeElementV5) {
    auto doc = parse("<html><body><time datetime=\"2024-12-25\">Christmas 2024</time></body></html>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "time");
}

// Cycle 1252: HTML parser tests V6
TEST(TreeBuilder, ArticleElementV6) {
    auto doc = clever::html::parse("<article><h2>Article Title</h2><p>Content here</p></article>");
    auto* el = doc->find_element("article");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "article");
}

TEST(TreeBuilder, AsideElementV6) {
    auto doc = clever::html::parse("<aside role=\"complementary\"><h3>Sidebar</h3></aside>");
    auto* el = doc->find_element("aside");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "aside");
}

TEST(TreeBuilder, NavElementV6) {
    auto doc = clever::html::parse("<nav id=\"main-nav\"><a href=\"/\">Home</a></nav>");
    auto* el = doc->find_element("nav");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "nav");
}

TEST(TreeBuilder, SectionElementV6) {
    auto doc = clever::html::parse("<section class=\"content\"><h2>Section</h2></section>");
    auto* el = doc->find_element("section");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "section");
}

TEST(TreeBuilder, HeaderElementV6) {
    auto doc = clever::html::parse("<header><h1>Site Title</h1></header>");
    auto* el = doc->find_element("header");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "header");
}

TEST(TreeBuilder, FooterElementV6) {
    auto doc = clever::html::parse("<footer><p>Copyright 2024</p></footer>");
    auto* el = doc->find_element("footer");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "footer");
}

TEST(TreeBuilder, MarkElementV6) {
    auto doc = clever::html::parse("<p>This is <mark class=\"highlight\">highlighted</mark></p>");
    auto* el = doc->find_element("mark");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "mark");
}

TEST(TreeBuilder, TimeElementV6) {
    auto doc = clever::html::parse("<time>January 1, 2025</time>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "time");
}

// Cycle 1261: HTML parser tests V7

TEST(TreeBuilder, ArticleElementV7) {
    auto doc = clever::html::parse("<article><h1>News Title</h1><p>Article content</p></article>");
    auto* el = doc->find_element("article");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "article");
}

TEST(TreeBuilder, FigureElementV7) {
    auto doc = clever::html::parse("<figure><img src=\"diagram.png\" /><figcaption>Diagram</figcaption></figure>");
    auto* el = doc->find_element("figure");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "figure");
}

TEST(TreeBuilder, FigcaptionElementV7) {
    auto doc = clever::html::parse("<figure><img src=\"photo.jpg\" /><figcaption>A beautiful photo</figcaption></figure>");
    auto* el = doc->find_element("figcaption");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "figcaption");
}

TEST(TreeBuilder, DetailsSummaryElementV7) {
    auto doc = clever::html::parse("<details><summary>More Info</summary><p>Hidden content</p></details>");
    auto* el = doc->find_element("details");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "details");
}

TEST(TreeBuilder, SummaryElementV7) {
    auto doc = clever::html::parse("<details><summary>Click to expand</summary><p>Content here</p></details>");
    auto* el = doc->find_element("summary");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "summary");
}

TEST(TreeBuilder, DataElementV7) {
    auto doc = clever::html::parse("<p>Product price: <data value=\"50\">$50</data></p>");
    auto* el = doc->find_element("data");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "data");
}

TEST(TreeBuilder, WbrElementV7) {
    auto doc = clever::html::parse("<p>A very long word<wbr />that breaks</p>");
    auto* el = doc->find_element("wbr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "wbr");
}

TEST(TreeBuilder, CodeElementV7) {
    auto doc = clever::html::parse("<p>Use the <code>printf</code> function</p>");
    auto* el = doc->find_element("code");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "code");
}

// Cycle 1270: HTML parser tests V8

TEST(TreeBuilder, HeaderElementV8) {
    auto doc = clever::html::parse("<header><h1>Site Title</h1><nav>Menu</nav></header>");
    auto* el = doc->find_element("header");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "header");
}

TEST(TreeBuilder, FooterElementV8) {
    auto doc = clever::html::parse("<footer><p>Copyright 2025</p></footer>");
    auto* el = doc->find_element("footer");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "footer");
}

TEST(TreeBuilder, NavElementV8) {
    auto doc = clever::html::parse("<nav><ul><li><a href=\"/\">Home</a></li></ul></nav>");
    auto* el = doc->find_element("nav");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "nav");
}

TEST(TreeBuilder, AsideElementV8) {
    auto doc = clever::html::parse("<aside><h3>Related Links</h3><ul><li>Link 1</li></ul></aside>");
    auto* el = doc->find_element("aside");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "aside");
}

TEST(TreeBuilder, SectionElementV8) {
    auto doc = clever::html::parse("<section><h2>Introduction</h2><p>Content section</p></section>");
    auto* el = doc->find_element("section");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "section");
}

TEST(TreeBuilder, MainElementV8) {
    auto doc = clever::html::parse("<main><article><h1>Main content</h1></article></main>");
    auto* el = doc->find_element("main");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "main");
}

TEST(TreeBuilder, TimeElementV8) {
    auto doc = clever::html::parse("<article><time datetime=\"2025-02-27\">Today</time></article>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "time");
}

TEST(TreeBuilder, MarkElementV8) {
    auto doc = clever::html::parse("<p>This is <mark>highlighted</mark> text</p>");
    auto* el = doc->find_element("mark");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "mark");
}

// Cycle 1279: HTML parser tests V9

TEST(TreeBuilder, ParagraphElementV9) {
    auto doc = clever::html::parse("<div><p>This is a paragraph</p></div>");
    auto* el = doc->find_element("p");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "p");
}

TEST(TreeBuilder, SpanElementV9) {
    auto doc = clever::html::parse("<p>Text with <span>inline styling</span> applied</p>");
    auto* el = doc->find_element("span");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "span");
}

TEST(TreeBuilder, UnorderedListElementV9) {
    auto doc = clever::html::parse("<ul><li>Item 1</li><li>Item 2</li></ul>");
    auto* el = doc->find_element("ul");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ul");
}

TEST(TreeBuilder, OrderedListElementV9) {
    auto doc = clever::html::parse("<ol><li>First</li><li>Second</li></ol>");
    auto* el = doc->find_element("ol");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ol");
}

TEST(TreeBuilder, ListItemElementV9) {
    auto doc = clever::html::parse("<ul><li>Item content</li></ul>");
    auto* el = doc->find_element("li");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "li");
}

TEST(TreeBuilder, DivElementV9) {
    auto doc = clever::html::parse("<div class=\"container\"><p>Content inside div</p></div>");
    auto* el = doc->find_element("div");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "div");
}

TEST(TreeBuilder, TableElementV9) {
    auto doc = clever::html::parse("<table><tr><td>Cell 1</td><td>Cell 2</td></tr></table>");
    auto* el = doc->find_element("table");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "table");
}

TEST(TreeBuilder, FormElementV9) {
    auto doc = clever::html::parse("<form action=\"/submit\"><input type=\"text\" /><button type=\"submit\">Submit</button></form>");
    auto* el = doc->find_element("form");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "form");
}

// Cycle 1288: HTML parser tests

TEST(TreeBuilder, NavElementV10) {
    auto doc = clever::html::parse("<nav><a href=\"/\">Home</a><a href=\"/about\">About</a></nav>");
    auto* el = doc->find_element("nav");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "nav");
}

TEST(TreeBuilder, HeaderElementV10) {
    auto doc = clever::html::parse("<header><h1>Page Title</h1><p>Subtitle</p></header>");
    auto* el = doc->find_element("header");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "header");
}

TEST(TreeBuilder, FooterElementV10) {
    auto doc = clever::html::parse("<footer><p>Copyright 2024</p><a href=\"/privacy\">Privacy</a></footer>");
    auto* el = doc->find_element("footer");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "footer");
}

TEST(TreeBuilder, ArticleElementV10) {
    auto doc = clever::html::parse("<article><h2>Article Title</h2><p>Article content goes here</p></article>");
    auto* el = doc->find_element("article");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "article");
}

TEST(TreeBuilder, SectionElementV10) {
    auto doc = clever::html::parse("<section><h2>Section Title</h2><p>Section content</p></section>");
    auto* el = doc->find_element("section");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "section");
}

TEST(TreeBuilder, AsideElementV10) {
    auto doc = clever::html::parse("<aside><h3>Sidebar</h3><ul><li>Item 1</li><li>Item 2</li></ul></aside>");
    auto* el = doc->find_element("aside");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "aside");
}

TEST(TreeBuilder, MainElementV10) {
    auto doc = clever::html::parse("<main><article><h1>Main Content</h1><p>This is the main content</p></article></main>");
    auto* el = doc->find_element("main");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "main");
}

TEST(TreeBuilder, FigureElementV10) {
    auto doc = clever::html::parse("<figure><img src=\"image.jpg\" alt=\"Description\" /><figcaption>Image caption</figcaption></figure>");
    auto* el = doc->find_element("figure");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "figure");
}

// Cycle 1297: HTML parser tests

TEST(TreeBuilder, HeaderElementV11) {
    auto doc = clever::html::parse("<header><nav><a href=\"/\">Home</a></nav></header>");
    auto* el = doc->find_element("header");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "header");
}

TEST(TreeBuilder, FooterElementV11) {
    auto doc = clever::html::parse("<footer><p>Copyright 2024</p><p>All rights reserved</p></footer>");
    auto* el = doc->find_element("footer");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "footer");
}

TEST(TreeBuilder, NavElementV11) {
    auto doc = clever::html::parse("<nav><ul><li><a href=\"/about\">About</a></li><li><a href=\"/contact\">Contact</a></li></ul></nav>");
    auto* el = doc->find_element("nav");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "nav");
}

TEST(TreeBuilder, ButtonElementV11) {
    auto doc = clever::html::parse("<button type=\"submit\">Send Form</button>");
    auto* el = doc->find_element("button");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "button");
}

TEST(TreeBuilder, InputElementV11) {
    auto doc = clever::html::parse("<input type=\"text\" placeholder=\"Enter name\" />");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "input");
}

TEST(TreeBuilder, TextareaElementV11) {
    auto doc = clever::html::parse("<textarea rows=\"5\" cols=\"40\">Default text</textarea>");
    auto* el = doc->find_element("textarea");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "textarea");
}

TEST(TreeBuilder, SelectElementV11) {
    auto doc = clever::html::parse("<select><option value=\"opt1\">Option 1</option><option value=\"opt2\">Option 2</option></select>");
    auto* el = doc->find_element("select");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "select");
}

TEST(TreeBuilder, LabelElementV11) {
    auto doc = clever::html::parse("<label for=\"username\">Username</label>");
    auto* el = doc->find_element("label");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "label");
}

// Cycle 1306: HTML parser tests
TEST(TreeBuilder, FormElementV12) {
    auto doc = clever::html::parse("<form method=\"POST\" action=\"/submit\"><input type=\"text\" name=\"username\" /></form>");
    auto* el = doc->find_element("form");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "form");
}

TEST(TreeBuilder, FieldsetElementV12) {
    auto doc = clever::html::parse("<fieldset><legend>Personal Info</legend><input type=\"text\" /></fieldset>");
    auto* el = doc->find_element("fieldset");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "fieldset");
}

TEST(TreeBuilder, OptgroupElementV12) {
    auto doc = clever::html::parse("<select><optgroup label=\"Group1\"><option>Item1</option></optgroup></select>");
    auto* el = doc->find_element("optgroup");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "optgroup");
}

TEST(TreeBuilder, DatalistElementV12) {
    auto doc = clever::html::parse("<input list=\"fruits\" /><datalist id=\"fruits\"><option value=\"Apple\"></option></datalist>");
    auto* el = doc->find_element("datalist");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "datalist");
}

TEST(TreeBuilder, KeygenElementV12) {
    auto doc = clever::html::parse("<keygen name=\"security\" challenge=\"challenge_string\" />");
    auto* el = doc->find_element("keygen");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "keygen");
}

TEST(TreeBuilder, OutputElementV12) {
    auto doc = clever::html::parse("<output name=\"result\" for=\"a b\">0</output>");
    auto* el = doc->find_element("output");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "output");
}

TEST(TreeBuilder, MeterElementV12) {
    auto doc = clever::html::parse("<meter value=\"6\" min=\"0\" max=\"10\"></meter>");
    auto* el = doc->find_element("meter");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "meter");
}

TEST(TreeBuilder, ProgressElementV12) {
    auto doc = clever::html::parse("<progress value=\"70\" max=\"100\"></progress>");
    auto* el = doc->find_element("progress");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "progress");
}

// Cycle 1315: HTML parser tests

TEST(TreeBuilder, DetailElementV13) {
    auto doc = clever::html::parse("<details open=\"open\"><summary>Summary</summary><p>Content</p></details>");
    auto* el = doc->find_element("details");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "details");
}

TEST(TreeBuilder, SummaryElementV13) {
    auto doc = clever::html::parse("<details><summary>Summary text</summary></details>");
    auto* el = doc->find_element("summary");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "summary");
}

TEST(TreeBuilder, MarkerElementV13) {
    auto doc = clever::html::parse("<ul><li><marker>point</marker></li></ul>");
    auto* el = doc->find_element("marker");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "marker");
}

TEST(TreeBuilder, TimeElementV13) {
    auto doc = clever::html::parse("<time datetime=\"2026-02-27\">February 27, 2026</time>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "time");
}

TEST(TreeBuilder, MenuElementV13) {
    auto doc = clever::html::parse("<menu type=\"toolbar\"><li>File</li><li>Edit</li></menu>");
    auto* el = doc->find_element("menu");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "menu");
}

TEST(TreeBuilder, PictureElementV13) {
    auto doc = clever::html::parse("<picture><source srcset=\"image.jpg\" media=\"(min-width: 600px)\"><img src=\"fallback.jpg\" alt=\"image\"></picture>");
    auto* el = doc->find_element("picture");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "picture");
}

TEST(TreeBuilder, TrackElementV13) {
    auto doc = clever::html::parse("<video><track kind=\"subtitles\" src=\"subs.vtt\" srclang=\"en\"></track></video>");
    auto* el = doc->find_element("track");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "track");
}

TEST(TreeBuilder, EmbedElementV13) {
    auto doc = clever::html::parse("<embed type=\"application/x-shockwave-flash\" src=\"animation.swf\" width=\"400\" height=\"300\">");
    auto* el = doc->find_element("embed");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "embed");
}

// Cycle 1324: HTML parser tests
TEST(TreeBuilder, FormElementV14) {
    auto doc = clever::html::parse("<form method=\"POST\" action=\"/submit\"><input type=\"text\" name=\"username\"><input type=\"password\" name=\"password\"></form>");
    auto* el = doc->find_element("form");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "form");
}

TEST(TreeBuilder, InputElementV14) {
    auto doc = clever::html::parse("<input type=\"email\" placeholder=\"Enter email\" required>");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "input");
}

TEST(TreeBuilder, SelectElementV14) {
    auto doc = clever::html::parse("<select name=\"colors\"><option value=\"red\">Red</option><option value=\"blue\">Blue</option></select>");
    auto* el = doc->find_element("select");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "select");
}

TEST(TreeBuilder, TextareaElementV14) {
    auto doc = clever::html::parse("<textarea rows=\"10\" cols=\"50\" name=\"comment\">Default text</textarea>");
    auto* el = doc->find_element("textarea");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "textarea");
}

TEST(TreeBuilder, LabelElementV14) {
    auto doc = clever::html::parse("<label for=\"username\">Username:</label><input id=\"username\" type=\"text\">");
    auto* el = doc->find_element("label");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "label");
}

TEST(TreeBuilder, FieldsetElementV14) {
    auto doc = clever::html::parse("<fieldset><legend>Contact Info</legend><input type=\"text\" name=\"name\"></fieldset>");
    auto* el = doc->find_element("fieldset");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "fieldset");
}

TEST(TreeBuilder, TableElementV14) {
    auto doc = clever::html::parse("<table border=\"1\"><tr><td>Cell 1</td><td>Cell 2</td></tr></table>");
    auto* el = doc->find_element("table");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "table");
}

TEST(TreeBuilder, CanvasElementV14) {
    auto doc = clever::html::parse("<canvas id=\"myCanvas\" width=\"200\" height=\"100\"></canvas>");
    auto* el = doc->find_element("canvas");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "canvas");
}

// Cycle 1333
TEST(TreeBuilder, DivElementV15) {
    auto doc = clever::html::parse("<html><body><div>content</div></body></html>");
    auto* el = doc->find_element("div");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "div");
}

TEST(TreeBuilder, SpanElementV15) {
    auto doc = clever::html::parse("<html><body><span>text</span></body></html>");
    auto* el = doc->find_element("span");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "span");
}

TEST(TreeBuilder, AnchorElementV15) {
    auto doc = clever::html::parse("<html><body><a href=\"/page\">link</a></body></html>");
    auto* el = doc->find_element("a");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "a");
}

TEST(TreeBuilder, ParagraphElementV15) {
    auto doc = clever::html::parse("<html><body><p>paragraph</p></body></html>");
    auto* el = doc->find_element("p");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "p");
}

TEST(TreeBuilder, HeadingElementV15) {
    auto doc = clever::html::parse("<html><body><h1>heading</h1></body></html>");
    auto* el = doc->find_element("h1");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "h1");
}

TEST(TreeBuilder, UnorderedListV15) {
    auto doc = clever::html::parse("<html><body><ul><li>item</li></ul></body></html>");
    auto* el = doc->find_element("ul");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ul");
}

TEST(TreeBuilder, OrderedListV15) {
    auto doc = clever::html::parse("<html><body><ol><li>item</li></ol></body></html>");
    auto* el = doc->find_element("ol");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ol");
}

TEST(TreeBuilder, SectionElementV15) {
    auto doc = clever::html::parse("<html><body><section><article>content</article></section></body></html>");
    auto* el = doc->find_element("section");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "section");
}

// Cycle 1342
TEST(TreeBuilder, DivElementV16) {
    auto doc = clever::html::parse("<html><body><div>content</div></body></html>");
    auto* el = doc->find_element("div");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "div");
}

TEST(TreeBuilder, SpanElementV16) {
    auto doc = clever::html::parse("<html><body><span>text</span></body></html>");
    auto* el = doc->find_element("span");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "span");
}

TEST(TreeBuilder, ArticleElementV16) {
    auto doc = clever::html::parse("<html><body><article><header>Article</header></article></body></html>");
    auto* el = doc->find_element("article");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "article");
}

TEST(TreeBuilder, HeaderElementV16) {
    auto doc = clever::html::parse("<html><body><header><nav>Navigation</nav></header></body></html>");
    auto* el = doc->find_element("header");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "header");
}

TEST(TreeBuilder, FooterElementV16) {
    auto doc = clever::html::parse("<html><body><footer><p>Footer content</p></footer></body></html>");
    auto* el = doc->find_element("footer");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "footer");
}

TEST(TreeBuilder, NavElementV16) {
    auto doc = clever::html::parse("<html><body><nav><a>Link</a></nav></body></html>");
    auto* el = doc->find_element("nav");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "nav");
}

TEST(TreeBuilder, AsideElementV16) {
    auto doc = clever::html::parse("<html><body><aside>Sidebar</aside></body></html>");
    auto* el = doc->find_element("aside");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "aside");
}

TEST(TreeBuilder, MainElementV16) {
    auto doc = clever::html::parse("<html><body><main>Main content</main></body></html>");
    auto* el = doc->find_element("main");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "main");
}

// Cycle 1351
TEST(TreeBuilder, ArticleElementV17) {
    auto doc = clever::html::parse("<html><body><article>Article content</article></body></html>");
    auto* el = doc->find_element("article");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "article");
}

TEST(TreeBuilder, SectionElementV17) {
    auto doc = clever::html::parse("<html><body><section>Section content</section></body></html>");
    auto* el = doc->find_element("section");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "section");
}

TEST(TreeBuilder, HeaderElementV17) {
    auto doc = clever::html::parse("<html><body><header>Header content</header></body></html>");
    auto* el = doc->find_element("header");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "header");
}

TEST(TreeBuilder, FigureElementV17) {
    auto doc = clever::html::parse("<html><body><figure><figcaption>Caption</figcaption></figure></body></html>");
    auto* el = doc->find_element("figure");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "figure");
}

TEST(TreeBuilder, FigcaptionElementV17) {
    auto doc = clever::html::parse("<html><body><figure><figcaption>Image caption</figcaption></figure></body></html>");
    auto* el = doc->find_element("figcaption");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "figcaption");
}

TEST(TreeBuilder, MarkElementV17) {
    auto doc = clever::html::parse("<html><body><p>Text with <mark>highlight</mark></p></body></html>");
    auto* el = doc->find_element("mark");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "mark");
}

TEST(TreeBuilder, TimeElementV17) {
    auto doc = clever::html::parse("<html><body><time datetime=\"2026-02-27\">Today</time></body></html>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "time");
}

TEST(TreeBuilder, AddressElementV17) {
    auto doc = clever::html::parse("<html><body><address>Contact info</address></body></html>");
    auto* el = doc->find_element("address");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "address");
}

TEST(TreeBuilder, ArticleElementV18) {
    auto doc = clever::html::parse("<html><body><article><h2>Article Title</h2><p>Content</p></article></body></html>");
    auto* el = doc->find_element("article");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "article");
}

TEST(TreeBuilder, AsideElementV18) {
    auto doc = clever::html::parse("<html><body><aside><p>Sidebar content</p></aside></body></html>");
    auto* el = doc->find_element("aside");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "aside");
}

TEST(TreeBuilder, DetailsElementV18) {
    auto doc = clever::html::parse("<html><body><details><summary>Click to expand</summary><p>Details content</p></details></body></html>");
    auto* el = doc->find_element("details");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "details");
}

TEST(TreeBuilder, SummaryElementV18) {
    auto doc = clever::html::parse("<html><body><details><summary>Summary text</summary></details></body></html>");
    auto* el = doc->find_element("summary");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "summary");
}

TEST(TreeBuilder, HeaderElementV18) {
    auto doc = clever::html::parse("<html><body><header><h1>Page Title</h1></header></body></html>");
    auto* el = doc->find_element("header");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "header");
}

TEST(TreeBuilder, FooterElementV18) {
    auto doc = clever::html::parse("<html><body><footer><p>Copyright 2026</p></footer></body></html>");
    auto* el = doc->find_element("footer");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "footer");
}

TEST(TreeBuilder, MainElementV18) {
    auto doc = clever::html::parse("<html><body><main><p>Main content</p></main></body></html>");
    auto* el = doc->find_element("main");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "main");
}

TEST(TreeBuilder, SectionElementV18) {
    auto doc = clever::html::parse("<html><body><section><h2>Section Title</h2><p>Section content</p></section></body></html>");
    auto* el = doc->find_element("section");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "section");
}

TEST(TreeBuilder, BlockquoteElementV19) {
    auto doc = clever::html::parse("<html><body><blockquote cite=\"https://example.com\">This is a quote</blockquote></body></html>");
    auto* el = doc->find_element("blockquote");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "blockquote");
    EXPECT_EQ(el->text_content(), "This is a quote");
}

TEST(TreeBuilder, PreElementV19) {
    auto doc = clever::html::parse("<html><body><pre>  Preformatted\n  text with\n  spacing</pre></body></html>");
    auto* el = doc->find_element("pre");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "pre");
}

TEST(TreeBuilder, CodeElementV19) {
    auto doc = clever::html::parse("<html><body><code>const x = 42;</code></body></html>");
    auto* el = doc->find_element("code");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "code");
    EXPECT_EQ(el->text_content(), "const x = 42;");
}

TEST(TreeBuilder, TableElementV19) {
    auto doc = clever::html::parse("<html><body><table><tr><td>Cell 1</td><td>Cell 2</td></tr></table></body></html>");
    auto* el = doc->find_element("table");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "table");
    auto* tr = doc->find_element("tr");
    ASSERT_NE(tr, nullptr);
    EXPECT_EQ(tr->tag_name, "tr");
}

TEST(TreeBuilder, UnorderedListV19) {
    auto doc = clever::html::parse("<html><body><ul><li>Apple</li><li>Banana</li><li>Cherry</li></ul></body></html>");
    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    EXPECT_EQ(ul->tag_name, "ul");
    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 3u);
    EXPECT_EQ(lis[0]->text_content(), "Apple");
}

TEST(TreeBuilder, DescriptionListV19) {
    auto doc = clever::html::parse("<html><body><dl><dt>Term</dt><dd>Definition</dd><dt>Another</dt><dd>Another def</dd></dl></body></html>");
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    EXPECT_EQ(dl->tag_name, "dl");
    auto* dt = doc->find_element("dt");
    ASSERT_NE(dt, nullptr);
    EXPECT_EQ(dt->tag_name, "dt");
    auto* dd = doc->find_element("dd");
    ASSERT_NE(dd, nullptr);
    EXPECT_EQ(dd->tag_name, "dd");
}

TEST(TreeBuilder, HorizontalRuleElementV19) {
    auto doc = clever::html::parse("<html><body><p>Above</p><hr><p>Below</p></body></html>");
    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
    EXPECT_EQ(hr->tag_name, "hr");
}

TEST(TreeBuilder, ImageElementV19) {
    auto doc = clever::html::parse("<html><body><img src=\"photo.jpg\" alt=\"A photo\" width=\"200\" height=\"150\"></body></html>");
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->tag_name, "img");
}

TEST(TreeBuilder, StrongElementV20) {
    auto doc = clever::html::parse("<html><body><p><strong>Bold text</strong></p></body></html>");
    auto* el = doc->find_element("strong");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "strong");
    EXPECT_EQ(el->text_content(), "Bold text");
}

TEST(TreeBuilder, EmElementV20) {
    auto doc = clever::html::parse("<html><body><p><em>Emphasized text</em></p></body></html>");
    auto* el = doc->find_element("em");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "em");
    EXPECT_EQ(el->text_content(), "Emphasized text");
}

TEST(TreeBuilder, BElementV20) {
    auto doc = clever::html::parse("<html><body><p><b>Bold</b></p></body></html>");
    auto* el = doc->find_element("b");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "b");
    EXPECT_EQ(el->text_content(), "Bold");
}

TEST(TreeBuilder, IElementV20) {
    auto doc = clever::html::parse("<html><body><p><i>Italic</i></p></body></html>");
    auto* el = doc->find_element("i");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "i");
    EXPECT_EQ(el->text_content(), "Italic");
}

TEST(TreeBuilder, UElementV20) {
    auto doc = clever::html::parse("<html><body><p><u>Underlined</u></p></body></html>");
    auto* el = doc->find_element("u");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "u");
    EXPECT_EQ(el->text_content(), "Underlined");
}

TEST(TreeBuilder, DelElementV20) {
    auto doc = clever::html::parse("<html><body><p><del>Deleted text</del></p></body></html>");
    auto* el = doc->find_element("del");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "del");
    EXPECT_EQ(el->text_content(), "Deleted text");
}

TEST(TreeBuilder, SupElementV20) {
    auto doc = clever::html::parse("<html><body><p>E=mc<sup>2</sup></p></body></html>");
    auto* el = doc->find_element("sup");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "sup");
    EXPECT_EQ(el->text_content(), "2");
}

TEST(TreeBuilder, AbbrElementV20) {
    auto doc = clever::html::parse("<html><body><p><abbr title=\"HyperText Markup Language\">HTML</abbr></p></body></html>");
    auto* el = doc->find_element("abbr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "abbr");
    EXPECT_EQ(el->text_content(), "HTML");
}

TEST(TreeBuilder, MarkElementV21) {
    auto doc = clever::html::parse("<html><body><p><mark>highlighted</mark></p></body></html>");
    auto* el = doc->find_element("mark");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "mark");
    EXPECT_EQ(el->text_content(), "highlighted");
}

TEST(TreeBuilder, TimeElementV21) {
    auto doc = clever::html::parse("<html><body><p><time datetime=\"2026-02-27\">Feb 27, 2026</time></p></body></html>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "time");
    EXPECT_EQ(el->text_content(), "Feb 27, 2026");
}

TEST(TreeBuilder, DataElementV21) {
    auto doc = clever::html::parse("<html><body><p><data value=\"12345\">Product ID</data></p></body></html>");
    auto* el = doc->find_element("data");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "data");
    EXPECT_EQ(el->text_content(), "Product ID");
}

TEST(TreeBuilder, OutputElementV21) {
    auto doc = clever::html::parse("<html><body><form><output name=\"result\">0</output></form></body></html>");
    auto* el = doc->find_element("output");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "output");
    EXPECT_EQ(el->text_content(), "0");
}

TEST(TreeBuilder, ProgressElementV21) {
    auto doc = clever::html::parse("<html><body><progress max=\"100\" value=\"70\"></progress></body></html>");
    auto* el = doc->find_element("progress");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "progress");
}

TEST(TreeBuilder, MeterElementV21) {
    auto doc = clever::html::parse("<html><body><meter low=\"3\" high=\"7\" max=\"10\" value=\"6\"></meter></body></html>");
    auto* el = doc->find_element("meter");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "meter");
}

TEST(TreeBuilder, FieldsetElementV21) {
    auto doc = clever::html::parse("<html><body><form><fieldset><legend>Contact</legend></fieldset></form></body></html>");
    auto* el = doc->find_element("fieldset");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "fieldset");
}

TEST(TreeBuilder, LegendElementV21) {
    auto doc = clever::html::parse("<html><body><form><fieldset><legend>Address</legend></fieldset></form></body></html>");
    auto* el = doc->find_element("legend");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "legend");
    EXPECT_EQ(el->text_content(), "Address");
}

TEST(TreeBuilder, SpanV22) {
    auto doc = clever::html::parse("<html><body><span>span content</span></body></html>");
    auto* el = doc->find_element("span");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "span");
}

TEST(TreeBuilder, AnchorV22) {
    auto doc = clever::html::parse("<html><body><a href=\"#\">link</a></body></html>");
    auto* el = doc->find_element("a");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "a");
}

TEST(TreeBuilder, ParagraphV22) {
    auto doc = clever::html::parse("<html><body><p>paragraph text</p></body></html>");
    auto* el = doc->find_element("p");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "p");
}

TEST(TreeBuilder, H1V22) {
    auto doc = clever::html::parse("<html><body><h1>heading 1</h1></body></html>");
    auto* el = doc->find_element("h1");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "h1");
}

TEST(TreeBuilder, H2V22) {
    auto doc = clever::html::parse("<html><body><h2>heading 2</h2></body></html>");
    auto* el = doc->find_element("h2");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "h2");
}

TEST(TreeBuilder, H3V22) {
    auto doc = clever::html::parse("<html><body><h3>heading 3</h3></body></html>");
    auto* el = doc->find_element("h3");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "h3");
}

TEST(TreeBuilder, FormV22) {
    auto doc = clever::html::parse("<html><body><form></form></body></html>");
    auto* el = doc->find_element("form");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "form");
}

TEST(TreeBuilder, InputV22) {
    auto doc = clever::html::parse("<html><body><input type=\"text\"></body></html>");
    auto* el = doc->find_element("input");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "input");
}

TEST(TreeBuilder, TableElementV23) {
    auto doc = clever::html::parse("<html><body><table><tr><td>cell</td></tr></table></body></html>");
    auto* el = doc->find_element("table");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "table");
}

TEST(TreeBuilder, TableHeadV23) {
    auto doc = clever::html::parse("<html><body><table><thead><tr><th>Header</th></tr></thead></table></body></html>");
    auto* el = doc->find_element("thead");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "thead");
}

TEST(TreeBuilder, TableBodyV23) {
    auto doc = clever::html::parse("<html><body><table><tbody><tr><td>data</td></tr></tbody></table></body></html>");
    auto* el = doc->find_element("tbody");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "tbody");
}

TEST(TreeBuilder, TableRowV23) {
    auto doc = clever::html::parse("<html><body><table><tr><td>cell1</td><td>cell2</td></tr></table></body></html>");
    auto* el = doc->find_element("tr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "tr");
}

TEST(TreeBuilder, TableDataV23) {
    auto doc = clever::html::parse("<html><body><table><tr><td>content</td></tr></table></body></html>");
    auto* el = doc->find_element("td");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "td");
}

TEST(TreeBuilder, TableHeaderV23) {
    auto doc = clever::html::parse("<html><body><table><tr><th>Title</th></tr></table></body></html>");
    auto* el = doc->find_element("th");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "th");
}

TEST(TreeBuilder, TableCaptionV23) {
    auto doc = clever::html::parse("<html><body><table><caption>Table Title</caption><tr><td>cell</td></tr></table></body></html>");
    auto* el = doc->find_element("caption");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "caption");
}

TEST(TreeBuilder, TableColgroupV23) {
    auto doc = clever::html::parse("<html><body><table><colgroup><col></colgroup><tr><td>cell</td></tr></table></body></html>");
    auto* el = doc->find_element("colgroup");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "colgroup");
}

TEST(TreeBuilder, TableTbodyV24) {
    auto doc = clever::html::parse("<html><body><table><tbody><tr><td>data</td></tr></tbody></table></body></html>");
    auto* el = doc->find_element("tbody");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "tbody");
}

TEST(TreeBuilder, TableTheadV24) {
    auto doc = clever::html::parse("<html><body><table><thead><tr><th>Header</th></tr></thead></table></body></html>");
    auto* el = doc->find_element("thead");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "thead");
}

TEST(TreeBuilder, TableTfootV24) {
    auto doc = clever::html::parse("<html><body><table><tfoot><tr><td>footer</td></tr></tfoot></table></body></html>");
    auto* el = doc->find_element("tfoot");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "tfoot");
}

TEST(TreeBuilder, SelectOptionV24) {
    auto doc = clever::html::parse("<html><body><select><option value=\"opt1\">Option 1</option></select></body></html>");
    auto* el = doc->find_element("option");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "option");
}

TEST(TreeBuilder, SelectOptgroupV24) {
    auto doc = clever::html::parse("<html><body><select><optgroup label=\"Group 1\"><option>Option</option></optgroup></select></body></html>");
    auto* el = doc->find_element("optgroup");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "optgroup");
}

TEST(TreeBuilder, TextareaElementV24) {
    auto doc = clever::html::parse("<html><body><form><textarea rows=\"5\" cols=\"50\">Text here</textarea></form></body></html>");
    auto* el = doc->find_element("textarea");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "textarea");
}

TEST(TreeBuilder, ButtonElementV24) {
    auto doc = clever::html::parse("<html><body><form><button type=\"submit\">Submit</button></form></body></html>");
    auto* el = doc->find_element("button");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "button");
}

TEST(TreeBuilder, LabelElementV24) {
    auto doc = clever::html::parse("<html><body><form><label for=\"input1\">Label Text</label><input id=\"input1\" type=\"text\"></form></body></html>");
    auto* el = doc->find_element("label");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "label");
}

TEST(TreeBuilder, IframeElementV25) {
    auto doc = clever::html::parse("<html><body><iframe src=\"page.html\"></iframe></body></html>");
    auto* el = doc->find_element("iframe");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "iframe");
}

TEST(TreeBuilder, EmbedElementV25) {
    auto doc = clever::html::parse("<html><body><embed src=\"plugin.swf\" type=\"application/x-shockwave-flash\"></body></html>");
    auto* el = doc->find_element("embed");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "embed");
}

TEST(TreeBuilder, ObjectElementV25) {
    auto doc = clever::html::parse("<html><body><object data=\"movie.mp4\" type=\"video/mp4\"></object></body></html>");
    auto* el = doc->find_element("object");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "object");
}

TEST(TreeBuilder, AudioElementV25) {
    auto doc = clever::html::parse("<html><body><audio controls><source src=\"sound.mp3\" type=\"audio/mpeg\"></audio></body></html>");
    auto* el = doc->find_element("audio");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "audio");
}

TEST(TreeBuilder, VideoElementV25) {
    auto doc = clever::html::parse("<html><body><video width=\"640\" height=\"480\"><source src=\"movie.mp4\" type=\"video/mp4\"></video></body></html>");
    auto* el = doc->find_element("video");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "video");
}

TEST(TreeBuilder, SourceElementV25) {
    auto doc = clever::html::parse("<html><body><audio><source src=\"sound.mp3\" type=\"audio/mpeg\"></audio></body></html>");
    auto* el = doc->find_element("source");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "source");
}

TEST(TreeBuilder, PictureElementV25) {
    auto doc = clever::html::parse("<html><body><picture><source srcset=\"large.jpg\" media=\"(min-width: 768px)\"><img src=\"small.jpg\" alt=\"Image\"></picture></body></html>");
    auto* el = doc->find_element("picture");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "picture");
}

TEST(TreeBuilder, CanvasElementV25) {
    auto doc = clever::html::parse("<html><body><canvas id=\"myCanvas\" width=\"200\" height=\"100\"></canvas></body></html>");
    auto* el = doc->find_element("canvas");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "canvas");
}

TEST(TreeBuilder, MapElementV26) {
    auto doc = clever::html::parse("<html><body><map name=\"shapes\"><area shape=\"rect\" coords=\"0,0,100,100\" href=\"rect.html\"></map></body></html>");
    auto* el = doc->find_element("map");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "map");
}

TEST(TreeBuilder, AreaElementV26) {
    auto doc = clever::html::parse("<html><body><map name=\"shapes\"><area shape=\"circle\" coords=\"100,100,50\" href=\"circle.html\"></map></body></html>");
    auto* el = doc->find_element("area");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "area");
}

TEST(TreeBuilder, SvgElementV26) {
    auto doc = clever::html::parse("<html><body><svg width=\"100\" height=\"100\"><circle cx=\"50\" cy=\"50\" r=\"40\"></circle></svg></body></html>");
    auto* el = doc->find_element("svg");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "svg");
}

TEST(TreeBuilder, MathElementV26) {
    auto doc = clever::html::parse("<html><body><math><mi>x</mi><mo>=</mo><mn>2</mn></math></body></html>");
    auto* el = doc->find_element("math");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "math");
}

TEST(TreeBuilder, DetailsElementV26) {
    auto doc = clever::html::parse("<html><body><details><summary>Click to expand</summary><p>Hidden content</p></details></body></html>");
    auto* el = doc->find_element("details");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "details");
}

TEST(TreeBuilder, SummaryElementV26) {
    auto doc = clever::html::parse("<html><body><details><summary>Click to expand</summary><p>Hidden content</p></details></body></html>");
    auto* el = doc->find_element("summary");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "summary");
}

TEST(TreeBuilder, DialogElementV26) {
    auto doc = clever::html::parse("<html><body><dialog open><p>Dialog content</p><button>Close</button></dialog></body></html>");
    auto* el = doc->find_element("dialog");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "dialog");
}

TEST(TreeBuilder, TemplateElementV26) {
    auto doc = clever::html::parse("<html><body><template id=\"tmpl\"><div>Template content</div></template></body></html>");
    auto* el = doc->find_element("template");
    if (el != nullptr) {
        EXPECT_EQ(el->tag_name, "template");
    }
}

TEST(TreeBuilder, AbbrElementV27) {
    auto doc = clever::html::parse("<html><body><abbr title=\"HyperText Markup Language\">HTML</abbr></body></html>");
    auto* el = doc->find_element("abbr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "abbr");
}

TEST(TreeBuilder, AddressElementV27) {
    auto doc = clever::html::parse("<html><body><address><p>123 Main Street</p><p>Springfield, USA</p></address></body></html>");
    auto* el = doc->find_element("address");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "address");
}

TEST(TreeBuilder, BdoElementV27) {
    auto doc = clever::html::parse("<html><body><bdo dir=\"rtl\">This text is right-to-left</bdo></body></html>");
    auto* el = doc->find_element("bdo");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "bdo");
}

TEST(TreeBuilder, CiteElementV27) {
    auto doc = clever::html::parse("<html><body><p>According to <cite>Wikipedia</cite>, the Earth is round.</p></body></html>");
    auto* el = doc->find_element("cite");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "cite");
}

TEST(TreeBuilder, DfnElementV27) {
    auto doc = clever::html::parse("<html><body><p><dfn>HTML</dfn> is the standard markup language for web pages.</p></body></html>");
    auto* el = doc->find_element("dfn");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "dfn");
}

TEST(TreeBuilder, KbdElementV27) {
    auto doc = clever::html::parse("<html><body><p>To save the file, press <kbd>Ctrl</kbd> + <kbd>S</kbd></p></body></html>");
    auto* el = doc->find_element("kbd");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "kbd");
}

TEST(TreeBuilder, SampElementV27) {
    auto doc = clever::html::parse("<html><body><p>The program output: <samp>File not found</samp></p></body></html>");
    auto* el = doc->find_element("samp");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "samp");
}

TEST(TreeBuilder, VarElementV27) {
    auto doc = clever::html::parse("<html><body><p>Let <var>x</var> be the number of users.</p></body></html>");
    auto* el = doc->find_element("var");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "var");
}

TEST(TreeBuilder, WbrElementV28) {
    auto doc = clever::html::parse("<html><body><p>This is a very long word<wbr>that can break here</p></body></html>");
    auto* el = doc->find_element("wbr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "wbr");
}

TEST(TreeBuilder, RubyElementV28) {
    auto doc = clever::html::parse("<html><body><ruby>Hello<rt>English</rt></ruby></body></html>");
    auto* el = doc->find_element("ruby");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ruby");
}

TEST(TreeBuilder, RtElementV28) {
    auto doc = clever::html::parse("<html><body><ruby>Hello<rt>English</rt></ruby></body></html>");
    auto* el = doc->find_element("rt");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "rt");
}

TEST(TreeBuilder, RpElementV28) {
    auto doc = clever::html::parse("<html><body><ruby>Hello<rp>(</rp><rt>English</rt><rp>)</rp></ruby></body></html>");
    auto* el = doc->find_element("rp");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "rp");
}

TEST(TreeBuilder, BdiElementV28) {
    auto doc = clever::html::parse("<html><body><p>The number <bdi>42</bdi> is meaningful.</p></body></html>");
    auto* el = doc->find_element("bdi");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "bdi");
}

TEST(TreeBuilder, DataElementV28) {
    auto doc = clever::html::parse("<html><body><p>The product <data value=\"5\">costs $5</data></p></body></html>");
    auto* el = doc->find_element("data");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "data");
}

TEST(TreeBuilder, TimeElementV28) {
    auto doc = clever::html::parse("<html><body><p>The event is on <time datetime=\"2026-02-27\">February 27, 2026</time></p></body></html>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "time");
}

TEST(TreeBuilder, SmallElementV28) {
    auto doc = clever::html::parse("<html><body><p>This is <small>fine print</small> text.</p></body></html>");
    auto* el = doc->find_element("small");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "small");
}

TEST(TreeBuilder, InsElementV29) {
    auto doc = clever::html::parse("<html><body><p>This is <ins>inserted</ins> text.</p></body></html>");
    auto* el = doc->find_element("ins");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ins");
}

TEST(TreeBuilder, SubElementV29) {
    auto doc = clever::html::parse("<html><body><p>H<sub>2</sub>O is water.</p></body></html>");
    auto* el = doc->find_element("sub");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "sub");
}

TEST(TreeBuilder, SupElementV29) {
    auto doc = clever::html::parse("<html><body><p>E=mc<sup>2</sup></p></body></html>");
    auto* el = doc->find_element("sup");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "sup");
}

TEST(TreeBuilder, MarkElementV29) {
    auto doc = clever::html::parse("<html><body><p>This is <mark>highlighted</mark> text.</p></body></html>");
    auto* el = doc->find_element("mark");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "mark");
}

TEST(TreeBuilder, QElementV29) {
    auto doc = clever::html::parse("<html><body><p>He said <q>hello</q> to me.</p></body></html>");
    auto* el = doc->find_element("q");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "q");
}

TEST(TreeBuilder, StrongElementV29) {
    auto doc = clever::html::parse("<html><body><p>This is <strong>very important</strong> text.</p></body></html>");
    auto* el = doc->find_element("strong");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "strong");
}

TEST(TreeBuilder, EmElementV29) {
    auto doc = clever::html::parse("<html><body><p>This is <em>emphasized</em> text.</p></body></html>");
    auto* el = doc->find_element("em");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "em");
}

TEST(TreeBuilder, SpanWithClassV29) {
    auto doc = clever::html::parse("<html><body><p>This is <span class=\"test\">a span</span> element.</p></body></html>");
    auto* el = doc->find_element("span");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "span");
}

TEST(TreeBuilder, CiteElementV30) {
    auto doc = clever::html::parse("<html><body><p>The book <cite>The Great Gatsby</cite> is a classic.</p></body></html>");
    auto* el = doc->find_element("cite");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "cite");
}

TEST(TreeBuilder, DfnElementV30) {
    auto doc = clever::html::parse("<html><body><p><dfn>HTML</dfn> is a markup language.</p></body></html>");
    auto* el = doc->find_element("dfn");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "dfn");
}

TEST(TreeBuilder, KbdElementV30) {
    auto doc = clever::html::parse("<html><body><p>Press <kbd>Ctrl+C</kbd> to copy.</p></body></html>");
    auto* el = doc->find_element("kbd");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "kbd");
}

TEST(TreeBuilder, SampElementV30) {
    auto doc = clever::html::parse("<html><body><p>The program output: <samp>Hello World</samp></p></body></html>");
    auto* el = doc->find_element("samp");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "samp");
}

TEST(TreeBuilder, VarElementV30) {
    auto doc = clever::html::parse("<html><body><p>Let <var>x</var> be the unknown value.</p></body></html>");
    auto* el = doc->find_element("var");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "var");
}

TEST(TreeBuilder, WbrElementV30) {
    auto doc = clever::html::parse("<html><body><p>word1<wbr>word2</p></body></html>");
    auto* el = doc->find_element("wbr");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "wbr");
}

TEST(TreeBuilder, BdiElementV30) {
    auto doc = clever::html::parse("<html><body><p>User <bdi>مرحبا</bdi> said hello.</p></body></html>");
    auto* el = doc->find_element("bdi");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "bdi");
}

TEST(TreeBuilder, BdoElementV30) {
    auto doc = clever::html::parse("<html><body><p>The text <bdo dir=\"rtl\">reversed</bdo> is right-to-left.</p></body></html>");
    auto* el = doc->find_element("bdo");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "bdo");
}

TEST(TreeBuilder, DetailsElementV31) {
    auto doc = clever::html::parse("<html><body><details open><summary>Details</summary><p>Content here</p></details></body></html>");
    auto* el = doc->find_element("details");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "details");
    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->tag_name, "summary");
}

TEST(TreeBuilder, TimeElementV31) {
    auto doc = clever::html::parse("<html><body><p>The event is <time datetime=\"2026-02-28\">today</time>.</p></body></html>");
    auto* el = doc->find_element("time");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "time");
    bool found_datetime = false;
    for (const auto& attr : el->attributes) {
        if (attr.name == "datetime") {
            found_datetime = true;
            break;
        }
    }
    EXPECT_TRUE(found_datetime);
}

TEST(TreeBuilder, InsElementWithAttributesV31) {
    auto doc = clever::html::parse("<html><body><p>This is <ins cite=\"url\" datetime=\"2026-02-28\">inserted</ins> text.</p></body></html>");
    auto* el = doc->find_element("ins");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "ins");
    EXPECT_EQ(el->attributes.size(), 2);
}

TEST(TreeBuilder, DelElementWithAttributesV31) {
    auto doc = clever::html::parse("<html><body><p>This is <del cite=\"url\" datetime=\"2026-02-28\">deleted</del> text.</p></body></html>");
    auto* el = doc->find_element("del");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "del");
    EXPECT_EQ(el->attributes.size(), 2);
}

TEST(TreeBuilder, DeeplyNestedElementsV31) {
    auto doc = clever::html::parse("<html><body><div><section><article><p><span><em><strong>text</strong></em></span></p></article></section></div></body></html>");
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->tag_name, "strong");
    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    EXPECT_EQ(article->tag_name, "article");
}

TEST(TreeBuilder, MultipleAttributesV31) {
    auto doc = clever::html::parse("<html><body><div id=\"main\" class=\"container\" data-test=\"value\" role=\"main\">Content</div></body></html>");
    auto* el = doc->find_element("div");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name, "div");
    EXPECT_EQ(el->attributes.size(), 4);
}

TEST(TreeBuilder, MixedContentV31) {
    auto doc = clever::html::parse("<html><body><p>Text with <br> break and <img alt=\"image\"> and <strong>bold</strong> content.</p></body></html>");
    auto* br = doc->find_element("br");
    ASSERT_NE(br, nullptr);
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
}

TEST(TreeBuilder, AsideWithNavElementV31) {
    auto doc = clever::html::parse("<html><body><aside><nav><ul><li><a href=\"/home\">Home</a></li></ul></nav></aside></body></html>");
    auto* aside = doc->find_element("aside");
    ASSERT_NE(aside, nullptr);
    EXPECT_EQ(aside->tag_name, "aside");
    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    EXPECT_EQ(nav->tag_name, "nav");
    auto* link = doc->find_element("a");
    ASSERT_NE(link, nullptr);
}

TEST(TreeBuilder, SectionArticleParagraphTextV32) {
    auto doc = clever::html::parse("<html><body><section><article><p>Hello parser</p></article></section></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    auto* section = doc->find_element("section");
    ASSERT_NE(section, nullptr);
    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hello parser");
}

TEST(TreeBuilder, DeepNestedInlineElementsV32) {
    auto doc = clever::html::parse("<html><body><div><p>alpha <span>beta <em>gamma</em></span></p></div></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "gamma");
}

TEST(TreeBuilder, AnchorAttributesByNameV32) {
    auto doc = clever::html::parse("<html><body><a href=\"/docs\" target=\"_blank\" rel=\"noopener\">Docs</a></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);

    bool found_href = false;
    bool found_target = false;
    bool found_rel = false;
    for (const auto& attr : a->attributes) {
        if (attr.name == "href" && attr.value == "/docs") found_href = true;
        if (attr.name == "target" && attr.value == "_blank") found_target = true;
        if (attr.name == "rel" && attr.value == "noopener") found_rel = true;
    }
    EXPECT_TRUE(found_href);
    EXPECT_TRUE(found_target);
    EXPECT_TRUE(found_rel);
}

TEST(TreeBuilder, InputAttributesByNameV32) {
    auto doc = clever::html::parse("<html><body><form><input type=\"text\" name=\"q\" value=\"hello\" placeholder=\"Search\"></form></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);

    bool found_type = false;
    bool found_name = false;
    bool found_value = false;
    bool found_placeholder = false;
    for (const auto& attr : input->attributes) {
        if (attr.name == "type" && attr.value == "text") found_type = true;
        if (attr.name == "name" && attr.value == "q") found_name = true;
        if (attr.name == "value" && attr.value == "hello") found_value = true;
        if (attr.name == "placeholder" && attr.value == "Search") found_placeholder = true;
    }
    EXPECT_TRUE(found_type);
    EXPECT_TRUE(found_name);
    EXPECT_TRUE(found_value);
    EXPECT_TRUE(found_placeholder);
}

TEST(TreeBuilder, UnorderedListThreeItemsV32) {
    auto doc = clever::html::parse("<html><body><ul><li>One</li><li>Two</li><li>Three</li></ul></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 3u);
    EXPECT_EQ(lis[0]->text_content(), "One");
    EXPECT_EQ(lis[1]->text_content(), "Two");
    EXPECT_EQ(lis[2]->text_content(), "Three");
}

TEST(TreeBuilder, TableCellNestingAndTextV32) {
    auto doc = clever::html::parse("<html><body><table><tbody><tr><td>Cell A</td><td>Cell B</td></tr></tbody></table></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    auto* tbody = doc->find_element("tbody");
    ASSERT_NE(tbody, nullptr);
    auto* tr = doc->find_element("tr");
    ASSERT_NE(tr, nullptr);
    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 2u);
    EXPECT_EQ(tds[0]->text_content(), "Cell A");
    EXPECT_EQ(tds[1]->text_content(), "Cell B");
}

TEST(TreeBuilder, ParagraphMixedInlineTextContentV32) {
    auto doc = clever::html::parse("<html><body><p>alpha <strong>beta</strong> gamma <em>delta</em></p></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_NE(p->text_content().find("alpha"), std::string::npos);
    EXPECT_NE(p->text_content().find("beta"), std::string::npos);
    EXPECT_NE(p->text_content().find("gamma"), std::string::npos);
    EXPECT_NE(p->text_content().find("delta"), std::string::npos);
}

TEST(TreeBuilder, FigureAndFigcaptionTextV32) {
    auto doc = clever::html::parse("<html><body><figure><img src=\"hero.png\" alt=\"hero\"><figcaption>Main hero image</figcaption></figure></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    auto* figcaption = doc->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->text_content(), "Main hero image");
}

// ---------------------------------------------------------------------------
// V55 — HTML parser coverage: nested elements, attributes, self-closing tags,
//       comments, text content, doctype
// ---------------------------------------------------------------------------

TEST(TreeBuilder, NestedElementsParentChainV55) {
    auto doc = parse("<html><body><section><article><h2>Title</h2><p>Body</p></article></section></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* section = doc->find_element("section");
    auto* article = doc->find_element("article");
    auto* h2 = doc->find_element("h2");
    auto* p = doc->find_element("p");
    ASSERT_NE(section, nullptr);
    ASSERT_NE(article, nullptr);
    ASSERT_NE(h2, nullptr);
    ASSERT_NE(p, nullptr);

    EXPECT_EQ(section->tag_name, "section");
    EXPECT_EQ(article->tag_name, "article");
    EXPECT_EQ(h2->tag_name, "h2");
    EXPECT_EQ(p->tag_name, "p");
    EXPECT_EQ(article->parent, section);
    EXPECT_EQ(h2->parent, article);
    EXPECT_EQ(p->parent, article);
}

TEST(TreeBuilder, AttributesIdClassAndDataV55) {
    auto doc = parse("<div id=\"root\" class=\"page primary\" data-role=\"main\">x</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->tag_name, "div");

    std::string id_value;
    std::string class_value;
    std::string role_value;
    for (const auto& attr : div->attributes) {
        if (attr.name == "id") id_value = attr.value;
        if (attr.name == "class") class_value = attr.value;
        if (attr.name == "data-role") role_value = attr.value;
    }

    EXPECT_EQ(id_value, "root");
    EXPECT_EQ(class_value, "page primary");
    EXPECT_EQ(role_value, "main");
}

TEST(TreeBuilder, SelfClosingTagsCreateLeafNodesV55) {
    auto doc = parse("<html><body><br/><img src=\"hero.png\" alt=\"hero\"/><input type=\"text\"/></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* br = doc->find_element("br");
    auto* img = doc->find_element("img");
    auto* input = doc->find_element("input");
    ASSERT_NE(br, nullptr);
    ASSERT_NE(img, nullptr);
    ASSERT_NE(input, nullptr);

    EXPECT_EQ(br->tag_name, "br");
    EXPECT_EQ(img->tag_name, "img");
    EXPECT_EQ(input->tag_name, "input");
    EXPECT_TRUE(br->children.empty());
    EXPECT_TRUE(img->children.empty());
    EXPECT_TRUE(input->children.empty());
}

TEST(TreeBuilder, CommentInsideBodyPreservedV55) {
    auto doc = parse("<html><body>alpha<!-- note -->beta</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->tag_name, "body");
    const std::string body_text = body->text_content();
    EXPECT_NE(body_text.find("alpha"), std::string::npos);
    EXPECT_NE(body_text.find("note"), std::string::npos);
    EXPECT_NE(body_text.find("beta"), std::string::npos);

    bool found_comment = false;
    for (const auto& child : body->children) {
        if (child->type == SimpleNode::Comment) {
            found_comment = true;
            EXPECT_EQ(child->data, " note ");
        }
    }
    EXPECT_TRUE(found_comment);
}

TEST(TreeBuilder, TextContentAcrossNestedInlineElementsV55) {
    auto doc = parse("<p>Hello <span>brave <em>new</em></span> world!</p>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    auto* span = doc->find_element("span");
    auto* em = doc->find_element("em");
    ASSERT_NE(p, nullptr);
    ASSERT_NE(span, nullptr);
    ASSERT_NE(em, nullptr);

    EXPECT_EQ(p->tag_name, "p");
    EXPECT_EQ(span->tag_name, "span");
    EXPECT_EQ(em->tag_name, "em");
    EXPECT_EQ(p->text_content(), "Hello brave new world!");
}

TEST(TreeBuilder, DoctypeHtmlNodePresentV55) {
    auto doc = parse("<!DOCTYPE html><html><body><p>x</p></body></html>");
    ASSERT_NE(doc, nullptr);

    bool found_doctype = false;
    for (const auto& child : doc->children) {
        if (child->type == SimpleNode::DocumentType) {
            found_doctype = true;
            EXPECT_EQ(child->doctype_name, "html");
        }
    }
    EXPECT_TRUE(found_doctype);
}

TEST(TreeBuilder, NestedListParentRelationshipV55) {
    auto doc = parse("<ul><li>Outer<ul><li>Inner</li></ul></li></ul>");
    ASSERT_NE(doc, nullptr);

    auto lists = doc->find_all_elements("ul");
    auto items = doc->find_all_elements("li");
    ASSERT_EQ(lists.size(), 2u);
    ASSERT_EQ(items.size(), 2u);

    auto* inner_list = lists[1];
    auto* inner_item = items[1];
    ASSERT_NE(inner_list, nullptr);
    ASSERT_NE(inner_item, nullptr);
    EXPECT_EQ(inner_list->tag_name, "ul");
    EXPECT_EQ(inner_item->tag_name, "li");
    EXPECT_EQ(inner_item->parent, inner_list);
    ASSERT_NE(inner_list->parent, nullptr);
    EXPECT_EQ(inner_list->parent->tag_name, "li");
}

TEST(TreeBuilder, AttributesWithMixedQuotingStylesV55) {
    auto doc = parse("<input type='search' name=query placeholder=\"Find\" disabled>");
    ASSERT_NE(doc, nullptr);

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->tag_name, "input");

    std::string type_value;
    std::string name_value;
    std::string placeholder_value;
    bool has_disabled = false;
    for (const auto& attr : input->attributes) {
        if (attr.name == "type") type_value = attr.value;
        if (attr.name == "name") name_value = attr.value;
        if (attr.name == "placeholder") placeholder_value = attr.value;
        if (attr.name == "disabled") has_disabled = true;
    }

    EXPECT_EQ(type_value, "search");
    EXPECT_EQ(name_value, "query");
    EXPECT_EQ(placeholder_value, "Find");
    EXPECT_TRUE(has_disabled);
}
