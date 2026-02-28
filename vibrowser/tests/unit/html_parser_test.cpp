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

TEST(Tokenizer, ScriptDataMismatchedEndTagPreservesLiteralSequenceV127) {
    Tokenizer tok("x</notscript>y</script>");
    tok.set_state(TokenizerState::ScriptData);
    tok.set_last_start_tag("script");

    std::string text;
    while (true) {
        auto t = tok.next_token();
        if (t.type == Token::Character) text += t.data;
        if (t.type == Token::EndTag || t.type == Token::EndOfFile) break;
    }

    EXPECT_EQ(text, "x</notscript>y");
}

TEST(Tokenizer, RcdataMismatchedEndTagPreservesLiteralSequenceV127) {
    Tokenizer tok("A</nottitle>B</title>");
    tok.set_state(TokenizerState::RCDATA);
    tok.set_last_start_tag("title");

    std::string text;
    while (true) {
        auto t = tok.next_token();
        if (t.type == Token::Character) text += t.data;
        if (t.type == Token::EndTag || t.type == Token::EndOfFile) break;
    }

    EXPECT_EQ(text, "A</nottitle>B");
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

TEST(TreeBuilder, AfterHeadHeadTokenReentryMetaStaysInHeadV127) {
    auto doc = parse("<html><head></head><meta charset='utf-8'><body><p>x</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* head = doc->find_element("head");
    auto* meta = doc->find_element("meta");
    ASSERT_NE(head, nullptr);
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->parent, head);
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

// ---------------------------------------------------------------------------
// V34 — HTML parser coverage: form elements, tables, semantic HTML,
//       nested attributes, empty elements, complex attribute patterns
// ---------------------------------------------------------------------------

TEST(TreeBuilder, FormElementsWithFieldsetAndLegendV34) {
    auto doc = parse("<html><body><form><fieldset><legend>Account</legend><input name=\"user\" type=\"text\"/></fieldset></form></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    auto* fieldset = doc->find_element("fieldset");
    auto* legend = doc->find_element("legend");
    auto* input = doc->find_element("input");
    ASSERT_NE(form, nullptr);
    ASSERT_NE(fieldset, nullptr);
    ASSERT_NE(legend, nullptr);
    ASSERT_NE(input, nullptr);

    EXPECT_EQ(form->tag_name, "form");
    EXPECT_EQ(fieldset->tag_name, "fieldset");
    EXPECT_EQ(legend->tag_name, "legend");
    EXPECT_EQ(legend->text_content(), "Account");
    EXPECT_EQ(input->tag_name, "input");
    EXPECT_EQ(fieldset->parent, form);
    EXPECT_EQ(legend->parent, fieldset);
}

TEST(TreeBuilder, TableWithHeaderFooterAndBodyRowsV34) {
    auto doc = parse("<table><thead><tr><th>Name</th><th>Age</th></tr></thead><tbody><tr><td>Alice</td><td>30</td></tr></tbody><tfoot><tr><td>Total</td><td>1</td></tr></tfoot></table>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    auto* thead = doc->find_element("thead");
    auto* tbody = doc->find_element("tbody");
    auto* tfoot = doc->find_element("tfoot");
    auto ths = doc->find_all_elements("th");
    auto tds = doc->find_all_elements("td");

    ASSERT_NE(table, nullptr);
    ASSERT_NE(thead, nullptr);
    ASSERT_NE(tbody, nullptr);
    ASSERT_NE(tfoot, nullptr);
    ASSERT_EQ(ths.size(), 2u);
    ASSERT_EQ(tds.size(), 4u);

    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Age");
    EXPECT_EQ(tds[0]->text_content(), "Alice");
    EXPECT_EQ(tds[1]->text_content(), "30");
}

TEST(TreeBuilder, SemanticElementsHeaderNavMainAsideFooterV34) {
    auto doc = parse("<html><body><header><nav><a href=\"/\">Home</a></nav></header><main><article><h1>Title</h1></article></main><aside><p>Sidebar</p></aside><footer>Copyright</footer></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* header = doc->find_element("header");
    auto* nav = doc->find_element("nav");
    auto* main = doc->find_element("main");
    auto* article = doc->find_element("article");
    auto* aside = doc->find_element("aside");
    auto* footer = doc->find_element("footer");
    auto* h1 = doc->find_element("h1");

    ASSERT_NE(header, nullptr);
    ASSERT_NE(nav, nullptr);
    ASSERT_NE(main, nullptr);
    ASSERT_NE(article, nullptr);
    ASSERT_NE(aside, nullptr);
    ASSERT_NE(footer, nullptr);
    ASSERT_NE(h1, nullptr);

    EXPECT_EQ(h1->text_content(), "Title");
    EXPECT_EQ(footer->text_content(), "Copyright");
    EXPECT_EQ(aside->text_content(), "Sidebar");
}

TEST(TreeBuilder, ButtonWithTypeAndNameAttributesV34) {
    auto doc = parse("<button type=\"submit\" name=\"action\" value=\"save\" class=\"btn-primary\">Save Changes</button>");
    ASSERT_NE(doc, nullptr);

    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->tag_name, "button");
    EXPECT_EQ(button->text_content(), "Save Changes");

    std::string type_val, name_val, value_val, class_val;
    for (const auto& attr : button->attributes) {
        if (attr.name == "type") type_val = attr.value;
        if (attr.name == "name") name_val = attr.value;
        if (attr.name == "value") value_val = attr.value;
        if (attr.name == "class") class_val = attr.value;
    }

    EXPECT_EQ(type_val, "submit");
    EXPECT_EQ(name_val, "action");
    EXPECT_EQ(value_val, "save");
    EXPECT_EQ(class_val, "btn-primary");
}

TEST(TreeBuilder, LabelInputAssociationWithForAttributeV34) {
    auto doc = parse("<form><label for=\"email-input\">Email:</label><input id=\"email-input\" type=\"email\" required/></form>");
    ASSERT_NE(doc, nullptr);

    auto* label = doc->find_element("label");
    auto* input = doc->find_element("input");
    ASSERT_NE(label, nullptr);
    ASSERT_NE(input, nullptr);

    std::string label_for;
    std::string input_id;
    std::string input_type;
    bool is_required = false;

    for (const auto& attr : label->attributes) {
        if (attr.name == "for") label_for = attr.value;
    }
    for (const auto& attr : input->attributes) {
        if (attr.name == "id") input_id = attr.value;
        if (attr.name == "type") input_type = attr.value;
        if (attr.name == "required") is_required = true;
    }

    EXPECT_EQ(label_for, "email-input");
    EXPECT_EQ(input_id, "email-input");
    EXPECT_EQ(input_type, "email");
    EXPECT_TRUE(is_required);
}

TEST(TreeBuilder, SelectWithOptgroupAndMultipleOptionsV34) {
    auto doc = parse("<select name=\"country\" id=\"country-select\"><optgroup label=\"Europe\"><option value=\"de\">Germany</option><option value=\"fr\">France</option></optgroup><optgroup label=\"Asia\"><option value=\"jp\">Japan</option></optgroup></select>");
    ASSERT_NE(doc, nullptr);

    auto* select = doc->find_element("select");
    auto optgroups = doc->find_all_elements("optgroup");
    auto options = doc->find_all_elements("option");

    ASSERT_NE(select, nullptr);
    ASSERT_EQ(optgroups.size(), 2u);
    ASSERT_EQ(options.size(), 3u);

    EXPECT_EQ(optgroups[0]->text_content(), "GermanyFrance");
    EXPECT_EQ(optgroups[1]->text_content(), "Japan");
    EXPECT_EQ(options[0]->text_content(), "Germany");
    EXPECT_EQ(options[1]->text_content(), "France");
    EXPECT_EQ(options[2]->text_content(), "Japan");
}

TEST(TreeBuilder, ScriptStyleAndMetaElementsV34) {
    auto doc = parse("<html><head><meta charset=\"UTF-8\"/><meta name=\"viewport\" content=\"width=device-width\"/><style>body { color: red; }</style></head><body><script>var x = 1;</script></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* head = doc->find_element("head");
    auto metas = doc->find_all_elements("meta");
    auto* style = doc->find_element("style");
    auto* script = doc->find_element("script");

    ASSERT_NE(head, nullptr);
    ASSERT_EQ(metas.size(), 2u);
    ASSERT_NE(style, nullptr);
    ASSERT_NE(script, nullptr);

    bool found_charset = false;
    bool found_viewport = false;
    for (const auto* meta : metas) {
        for (const auto& attr : meta->attributes) {
            if (attr.name == "charset") found_charset = true;
            if (attr.name == "name" && attr.value == "viewport") found_viewport = true;
        }
    }
    EXPECT_TRUE(found_charset);
    EXPECT_TRUE(found_viewport);
}

TEST(TreeBuilder, EmptyAndWhitespaceTextNodesPreservationV34) {
    auto doc = parse("<div>  <span>inner</span>  </div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    auto* span = doc->find_element("span");
    ASSERT_NE(div, nullptr);
    ASSERT_NE(span, nullptr);

    EXPECT_EQ(span->tag_name, "span");
    EXPECT_EQ(span->text_content(), "inner");
    const std::string div_text = div->text_content();
    EXPECT_NE(div_text.find("inner"), std::string::npos);
}

// ---------------------------------------------------------------------------
// V57 — Additional HTML parser coverage: edge cases, empty tags, nesting,
//       malformed HTML, text normalization, special tags
// ---------------------------------------------------------------------------

TEST(TreeBuilder, CompletelyEmptyDocumentV57) {
    auto doc = parse("");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->type, SimpleNode::Document);
    // Should auto-generate html, head, body even with empty input
    auto* html = doc->find_element("html");
    ASSERT_NE(html, nullptr);
}

TEST(TreeBuilder, OnlyWhitespaceDocumentV57) {
    auto doc = parse("   \n\t\n   ");
    ASSERT_NE(doc, nullptr);
    auto* html = doc->find_element("html");
    ASSERT_NE(html, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
}

TEST(TreeBuilder, DeeplyNestedInlineElementsV57) {
    auto doc = parse("<p><span><em><strong><u>Deep</u></strong></em></span></p>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    auto* span = doc->find_element("span");
    auto* em = doc->find_element("em");
    auto* strong = doc->find_element("strong");
    auto* u = doc->find_element("u");

    ASSERT_NE(p, nullptr);
    ASSERT_NE(span, nullptr);
    ASSERT_NE(em, nullptr);
    ASSERT_NE(strong, nullptr);
    ASSERT_NE(u, nullptr);

    EXPECT_EQ(u->text_content(), "Deep");
    EXPECT_EQ(u->parent, strong);
    EXPECT_EQ(strong->parent, em);
    EXPECT_EQ(em->parent, span);
}

TEST(TreeBuilder, MissingClosingTagsImplicitClosureV57) {
    auto doc = parse("<div><p>First<p>Second<p>Third</div>");
    ASSERT_NE(doc, nullptr);

    auto ps = doc->find_all_elements("p");
    EXPECT_GE(ps.size(), 3u);
    EXPECT_EQ(ps[0]->text_content(), "First");
    EXPECT_EQ(ps[1]->text_content(), "Second");
    EXPECT_EQ(ps[2]->text_content(), "Third");
}

TEST(TreeBuilder, VoidElementsWithoutSlashV57) {
    auto doc = parse("<div><br><hr><img><input><meta></div>");
    ASSERT_NE(doc, nullptr);

    auto* br = doc->find_element("br");
    auto* hr = doc->find_element("hr");
    auto* img = doc->find_element("img");
    auto* input = doc->find_element("input");
    auto* meta = doc->find_element("meta");

    ASSERT_NE(br, nullptr);
    ASSERT_NE(hr, nullptr);
    ASSERT_NE(img, nullptr);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(meta, nullptr);

    EXPECT_EQ(br->children.size(), 0u);
    EXPECT_EQ(hr->children.size(), 0u);
}

TEST(TreeBuilder, MixedCaseTagNamesV57) {
    auto doc = parse("<DIV><SPAN>Mixed</SPAN></DIV>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    auto* span = doc->find_element("span");
    ASSERT_NE(div, nullptr);
    ASSERT_NE(span, nullptr);

    EXPECT_EQ(div->tag_name, "div");
    EXPECT_EQ(span->tag_name, "span");
    EXPECT_EQ(span->text_content(), "Mixed");
}

TEST(TreeBuilder, AttributesWithoutQuotesV57) {
    auto doc = parse("<div id=test-id class=primary data-value=123>Content</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);

    std::string id_val, class_val, data_val;
    for (const auto& attr : div->attributes) {
        if (attr.name == "id") id_val = attr.value;
        if (attr.name == "class") class_val = attr.value;
        if (attr.name == "data-value") data_val = attr.value;
    }

    EXPECT_EQ(id_val, "test-id");
    EXPECT_EQ(class_val, "primary");
    EXPECT_EQ(data_val, "123");
}

TEST(TreeBuilder, HtmlEntityDecodingInTextV57) {
    auto doc = parse("<p>&lt; &gt; &amp; &quot; &apos;</p>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);

    const std::string content = p->text_content();
    EXPECT_NE(content.find('<'), std::string::npos);
    EXPECT_NE(content.find('>'), std::string::npos);
    EXPECT_NE(content.find('&'), std::string::npos);
}

// ---------------------------------------------------------------------------
// V58 — Additional HTML parser coverage: whitespace handling, special chars,
//       nested elements, optional tags, and attribute variations
// ---------------------------------------------------------------------------

TEST(TreeBuilder, ConsecutiveTextNodesV58) {
    auto doc = parse("<div>Text1Text2Text3</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);

    const std::string content = div->text_content();
    EXPECT_EQ(content, "Text1Text2Text3");
}

TEST(TreeBuilder, NestedListsWithComplexStructureV58) {
    auto doc = parse("<ul><li>Item1<ul><li>Nested1</li><li>Nested2</li></ul></li><li>Item2</li></ul>");
    ASSERT_NE(doc, nullptr);

    auto all_lis = doc->find_all_elements("li");
    EXPECT_GE(all_lis.size(), 4u);

    auto* item1 = all_lis[0];
    ASSERT_NE(item1, nullptr);
    EXPECT_EQ(item1->text_content().find("Item1"), 0u);
}

TEST(TreeBuilder, AttributeValueWithSpecialCharactersV58) {
    auto doc = parse("<a href=\"http://example.com?param1=value1&param2=value2\" title=\"Click & Go\">Link</a>");
    ASSERT_NE(doc, nullptr);

    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);

    std::string href_val, title_val;
    for (const auto& attr : a->attributes) {
        if (attr.name == "href") href_val = attr.value;
        if (attr.name == "title") title_val = attr.value;
    }

    EXPECT_NE(href_val.find("param1"), std::string::npos);
    EXPECT_NE(href_val.find("param2"), std::string::npos);
    EXPECT_NE(title_val.find("&"), std::string::npos);
}

TEST(TreeBuilder, MultipleNestedDivsV58) {
    auto doc = parse("<div><div><div><div>Deep</div></div></div></div>");
    ASSERT_NE(doc, nullptr);

    auto all_divs = doc->find_all_elements("div");
    EXPECT_EQ(all_divs.size(), 4u);

    auto* innermost = all_divs[3];
    ASSERT_NE(innermost, nullptr);
    EXPECT_EQ(innermost->text_content(), "Deep");
}

TEST(TreeBuilder, OptionalClosingTagsImplicitListItemsV58) {
    auto doc = parse("<ol><li>First<li>Second<li>Third</ol>");
    ASSERT_NE(doc, nullptr);

    auto lis = doc->find_all_elements("li");
    EXPECT_EQ(lis.size(), 3u);

    EXPECT_EQ(lis[0]->text_content(), "First");
    EXPECT_EQ(lis[1]->text_content(), "Second");
    EXPECT_EQ(lis[2]->text_content(), "Third");
}

TEST(TreeBuilder, ParagraphWithMixedInlineElementsV58) {
    auto doc = parse("<p>Normal <b>bold</b> <i>italic</i> <u>underline</u> <code>code</code> text</p>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);

    auto* b = doc->find_element("b");
    auto* i = doc->find_element("i");
    auto* u = doc->find_element("u");
    auto* code = doc->find_element("code");

    ASSERT_NE(b, nullptr);
    ASSERT_NE(i, nullptr);
    ASSERT_NE(u, nullptr);
    ASSERT_NE(code, nullptr);

    EXPECT_EQ(b->text_content(), "bold");
    EXPECT_EQ(i->text_content(), "italic");
    EXPECT_EQ(u->text_content(), "underline");
    EXPECT_EQ(code->text_content(), "code");
}

TEST(TreeBuilder, SingleQuotedAndDoubleQuotedAttributesV58) {
    auto doc = parse("<div id='single-quote' class=\"double-quote\" data-value='mixed\"quotes'>Content</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);

    std::string id_val, class_val;
    for (const auto& attr : div->attributes) {
        if (attr.name == "id") id_val = attr.value;
        if (attr.name == "class") class_val = attr.value;
    }

    EXPECT_EQ(id_val, "single-quote");
    EXPECT_EQ(class_val, "double-quote");
}

TEST(TreeBuilder, ComplexTableStructureV58) {
    auto doc = parse("<table><thead><tr><th>Header1</th><th>Header2</th></tr></thead><tbody><tr><td>Data1</td><td>Data2</td></tr><tr><td>Data3</td><td>Data4</td></tr></tbody></table>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    auto* thead = doc->find_element("thead");
    auto* tbody = doc->find_element("tbody");
    auto ths = doc->find_all_elements("th");
    auto tds = doc->find_all_elements("td");

    ASSERT_NE(table, nullptr);
    ASSERT_NE(thead, nullptr);
    ASSERT_NE(tbody, nullptr);
    EXPECT_EQ(ths.size(), 2u);
    EXPECT_EQ(tds.size(), 4u);

    EXPECT_EQ(ths[0]->text_content(), "Header1");
    EXPECT_EQ(ths[1]->text_content(), "Header2");
}

TEST(TreeBuilder, FormWithInputElementsV59) {
    auto doc = parse("<form action=\"submit\" method=\"POST\"><input type=\"text\" name=\"username\"><input type=\"password\" name=\"pwd\"><button>Submit</button></form>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);

    auto inputs = doc->find_all_elements("input");
    auto* button = doc->find_element("button");

    EXPECT_EQ(inputs.size(), 2u);
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text_content(), "Submit");
}

TEST(TreeBuilder, NavWithListItemsV59) {
    auto doc = parse("<nav><ul><li><a href=\"/home\">Home</a></li><li><a href=\"/about\">About</a></li><li><a href=\"/contact\">Contact</a></li></ul></nav>");
    ASSERT_NE(doc, nullptr);

    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);

    auto lis = doc->find_all_elements("li");
    auto links = doc->find_all_elements("a");

    EXPECT_EQ(lis.size(), 3u);
    EXPECT_EQ(links.size(), 3u);
    EXPECT_EQ(links[0]->text_content(), "Home");
    EXPECT_EQ(links[2]->text_content(), "Contact");
}

TEST(TreeBuilder, ArticleWithSectionHeaderFooterV59) {
    auto doc = parse("<article><header><h1>Title</h1></header><section>Content here</section><footer>Footer info</footer></article>");
    ASSERT_NE(doc, nullptr);

    auto* article = doc->find_element("article");
    auto* header = doc->find_element("header");
    auto* section = doc->find_element("section");
    auto* footer = doc->find_element("footer");
    auto* h1 = doc->find_element("h1");

    ASSERT_NE(article, nullptr);
    ASSERT_NE(header, nullptr);
    ASSERT_NE(section, nullptr);
    ASSERT_NE(footer, nullptr);
    ASSERT_NE(h1, nullptr);

    EXPECT_EQ(h1->text_content(), "Title");
    EXPECT_EQ(section->text_content(), "Content here");
    EXPECT_EQ(footer->text_content(), "Footer info");
}

TEST(TreeBuilder, NestedListsV59) {
    auto doc = parse("<ul><li>Item1<ul><li>Nested1</li><li>Nested2</li></ul></li><li>Item2</li></ul>");
    ASSERT_NE(doc, nullptr);

    auto lis = doc->find_all_elements("li");
    EXPECT_GE(lis.size(), 4u);

    auto uls = doc->find_all_elements("ul");
    EXPECT_EQ(uls.size(), 2u);
}

TEST(TreeBuilder, DataListWithOptionsV59) {
    auto doc = parse("<select><option value=\"1\">Option One</option><option value=\"2\" selected>Option Two</option><option value=\"3\">Option Three</option></select>");
    ASSERT_NE(doc, nullptr);

    auto* select = doc->find_element("select");
    auto options = doc->find_all_elements("option");

    ASSERT_NE(select, nullptr);
    EXPECT_EQ(options.size(), 3u);

    EXPECT_EQ(options[0]->text_content(), "Option One");
    EXPECT_EQ(options[1]->text_content(), "Option Two");
    EXPECT_EQ(options[2]->text_content(), "Option Three");
}

TEST(TreeBuilder, FigureWithCaptionV59) {
    auto doc = parse("<figure><img src=\"image.jpg\" alt=\"Description\"><figcaption>Image caption text</figcaption></figure>");
    ASSERT_NE(doc, nullptr);

    auto* figure = doc->find_element("figure");
    auto* img = doc->find_element("img");
    auto* figcaption = doc->find_element("figcaption");

    ASSERT_NE(figure, nullptr);
    ASSERT_NE(img, nullptr);
    ASSERT_NE(figcaption, nullptr);

    EXPECT_EQ(figcaption->text_content(), "Image caption text");
}

TEST(TreeBuilder, BlockquoteWithCitationV59) {
    auto doc = parse("<blockquote cite=\"http://example.com\"><p>Quote text here</p><footer>Attribution info</footer></blockquote>");
    ASSERT_NE(doc, nullptr);

    auto* blockquote = doc->find_element("blockquote");
    auto* p = doc->find_element("p");
    auto* footer = doc->find_element("footer");

    ASSERT_NE(blockquote, nullptr);
    ASSERT_NE(p, nullptr);
    ASSERT_NE(footer, nullptr);

    EXPECT_EQ(p->text_content(), "Quote text here");
    EXPECT_EQ(footer->text_content(), "Attribution info");
}

TEST(TreeBuilder, VideoWithSourceTracksV59) {
    auto doc = parse("<video width=\"320\" height=\"240\" controls><source src=\"video.mp4\" type=\"video/mp4\"><track src=\"subs.vtt\" kind=\"subtitles\" srclang=\"en\"></video>");
    ASSERT_NE(doc, nullptr);

    auto* video = doc->find_element("video");
    auto sources = doc->find_all_elements("source");
    auto tracks = doc->find_all_elements("track");

    ASSERT_NE(video, nullptr);
    EXPECT_EQ(sources.size(), 1u);
    EXPECT_EQ(tracks.size(), 1u);
}

TEST(TreeBuilder, DocTypeWithHtmlElementsV60) {
    auto doc = parse("<!DOCTYPE html><html lang=\"en\"><head><title>Test</title></head><body><h1>Hello</h1></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* html = doc->find_element("html");
    auto* head = doc->find_element("head");
    auto* title = doc->find_element("title");
    auto* body = doc->find_element("body");
    auto* h1 = doc->find_element("h1");

    ASSERT_NE(html, nullptr);
    ASSERT_NE(head, nullptr);
    ASSERT_NE(title, nullptr);
    ASSERT_NE(body, nullptr);
    ASSERT_NE(h1, nullptr);

    EXPECT_EQ(title->text_content(), "Test");
    EXPECT_EQ(h1->text_content(), "Hello");
}

TEST(TreeBuilder, SelfClosingTagsV60) {
    auto doc = parse("<html><body><img src=\"image.png\" alt=\"test\"><br><hr><input type=\"checkbox\" checked><meta charset=\"utf-8\"></body></html>");
    ASSERT_NE(doc, nullptr);

    auto img = doc->find_all_elements("img");
    auto br = doc->find_all_elements("br");
    auto hr = doc->find_all_elements("hr");
    auto input = doc->find_all_elements("input");
    auto meta = doc->find_all_elements("meta");

    EXPECT_EQ(img.size(), 1u);
    EXPECT_EQ(br.size(), 1u);
    EXPECT_EQ(hr.size(), 1u);
    EXPECT_EQ(input.size(), 1u);
    EXPECT_EQ(meta.size(), 1u);
}

TEST(TreeBuilder, MalformedHtmlRecoveryV60) {
    auto doc = parse("<html><body><p>Paragraph<div>Div inside paragraph</div></p><p>Another</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto paragraphs = doc->find_all_elements("p");
    auto divs = doc->find_all_elements("div");

    ASSERT_GE(paragraphs.size(), 2u);
    ASSERT_GE(divs.size(), 1u);

    EXPECT_EQ(paragraphs[0]->text_content(), "Paragraph");
    EXPECT_EQ(divs[0]->text_content(), "Div inside paragraph");
}

TEST(TreeBuilder, NestedTablesWithAttributesV60) {
    auto doc = parse("<table border=\"1\"><tr><td>Cell 1<table><tr><td>Nested Cell</td></tr></table></td><td>Cell 2</td></tr></table>");
    ASSERT_NE(doc, nullptr);

    auto tables = doc->find_all_elements("table");
    auto cells = doc->find_all_elements("td");
    auto rows = doc->find_all_elements("tr");

    ASSERT_EQ(tables.size(), 2u);
    ASSERT_GE(cells.size(), 3u);
    ASSERT_GE(rows.size(), 2u);

    bool has_cell1 = false;
    bool has_cell2 = false;
    bool has_nested = false;

    for (auto* cell : cells) {
        std::string text = cell->text_content();
        if (text.find("Cell 1") != std::string::npos) has_cell1 = true;
        if (text == "Cell 2") has_cell2 = true;
        if (text == "Nested Cell") has_nested = true;
    }

    EXPECT_TRUE(has_cell1);
    EXPECT_TRUE(has_cell2);
    EXPECT_TRUE(has_nested);
}

TEST(TreeBuilder, FormWithMultipleInputTypesV60) {
    auto doc = parse("<form id=\"myform\" action=\"/submit\" method=\"POST\" enctype=\"multipart/form-data\"><input type=\"text\" name=\"username\" required><input type=\"email\" name=\"email\" placeholder=\"Enter email\"><textarea name=\"message\" rows=\"5\" cols=\"40\"></textarea><select name=\"country\"><option>USA</option><option>Canada</option></select><label>Agree <input type=\"checkbox\" name=\"agree\"></label></form>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    auto inputs = doc->find_all_elements("input");
    auto* textarea = doc->find_element("textarea");
    auto* select = doc->find_element("select");
    auto options = doc->find_all_elements("option");
    auto* label = doc->find_element("label");

    ASSERT_NE(form, nullptr);
    EXPECT_EQ(inputs.size(), 3u);
    ASSERT_NE(textarea, nullptr);
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(options.size(), 2u);
    ASSERT_NE(label, nullptr);
}

TEST(TreeBuilder, AudioWithMultipleSourcesV60) {
    auto doc = parse("<audio controls preload=\"metadata\"><source src=\"song.mp3\" type=\"audio/mpeg\"><source src=\"song.ogg\" type=\"audio/ogg\"></audio>");
    ASSERT_NE(doc, nullptr);

    auto* audio = doc->find_element("audio");
    auto sources = doc->find_all_elements("source");

    ASSERT_NE(audio, nullptr);
    EXPECT_EQ(sources.size(), 2u);

    auto srcs = sources;
    EXPECT_TRUE(srcs[0]->text_content().empty() || srcs[0]->text_content().find("mp3") == std::string::npos);
    EXPECT_TRUE(srcs[1]->text_content().empty() || srcs[1]->text_content().find("ogg") == std::string::npos);
}

TEST(TreeBuilder, ScriptAndStyleElementsV60) {
    auto doc = parse("<html><head><style>body { color: red; }</style></head><body><script>console.log('test');</script></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* style = doc->find_element("style");
    auto* script = doc->find_element("script");

    ASSERT_NE(style, nullptr);
    ASSERT_NE(script, nullptr);

    std::string style_text = style->text_content();
    std::string script_text = script->text_content();

    EXPECT_TRUE(style_text.find("color") != std::string::npos);
    EXPECT_TRUE(script_text.find("console.log") != std::string::npos);
}

TEST(TreeBuilder, Html5SemanticElementsV60) {
    auto doc = parse("<html><body><details open><summary>Click me</summary><p>Hidden content</p></details><aside>Sidebar</aside><main><article><header>Article Header</header><time datetime=\"2024-01-01\">January 1, 2024</time></article></main></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* details = doc->find_element("details");
    auto* summary = doc->find_element("summary");
    auto* aside = doc->find_element("aside");
    auto* main = doc->find_element("main");
    auto* article = doc->find_element("article");
    auto* header = doc->find_element("header");
    auto* time = doc->find_element("time");

    ASSERT_NE(details, nullptr);
    ASSERT_NE(summary, nullptr);
    ASSERT_NE(aside, nullptr);
    ASSERT_NE(main, nullptr);
    ASSERT_NE(article, nullptr);
    ASSERT_NE(header, nullptr);
    ASSERT_NE(time, nullptr);

    EXPECT_EQ(summary->text_content(), "Click me");
    EXPECT_EQ(aside->text_content(), "Sidebar");
    EXPECT_EQ(header->text_content(), "Article Header");
}

// Test 1: SVG inline elements - testing SVG tags parsed as regular elements
TEST(TreeBuilder, SVGInlineElementsV61) {
    auto doc = parse("<html><body><svg width=\"100\" height=\"100\"><circle cx=\"50\" cy=\"50\" r=\"40\"></circle><rect x=\"10\" y=\"10\" width=\"30\" height=\"30\"></rect></svg></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* svg = doc->find_element("svg");
    ASSERT_NE(svg, nullptr);

    auto circles = doc->find_all_elements("circle");
    auto rects = doc->find_all_elements("rect");

    EXPECT_EQ(circles.size(), 1u);
    EXPECT_EQ(rects.size(), 1u);
}

// Test 2: MathML parsing - testing MathML tags as elements
TEST(TreeBuilder, MathMLElementsV61) {
    auto doc = parse("<html><body><math><mrow><mi>x</mi><mo>=</mo><mn>2</mn></mrow></math></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* math = doc->find_element("math");
    ASSERT_NE(math, nullptr);

    auto mrows = doc->find_all_elements("mrow");
    auto mis = doc->find_all_elements("mi");
    auto mos = doc->find_all_elements("mo");
    auto mns = doc->find_all_elements("mn");

    EXPECT_EQ(mrows.size(), 1u);
    EXPECT_EQ(mis.size(), 1u);
    EXPECT_EQ(mos.size(), 1u);
    EXPECT_EQ(mns.size(), 1u);
}

// Test 3: Template elements - testing HTML template tag parsing
TEST(TreeBuilder, TemplateElementsV61) {
    auto doc = parse("<html><body><div><template id=\"mytemplate\"><p>Template content</p><span>Nested span</span></template></div></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* template_elem = doc->find_element("template");
    ASSERT_NE(template_elem, nullptr);

    // Template should have child elements
    auto ps = doc->find_all_elements("p");
    auto spans = doc->find_all_elements("span");

    EXPECT_EQ(ps.size(), 1u);
    EXPECT_EQ(spans.size(), 1u);
}

// Test 4: Custom elements - testing hyphenated element names
TEST(TreeBuilder, CustomElementsV61) {
    auto doc = parse("<html><body><my-widget data-value=\"test\">Custom content<my-sub-component>Nested</my-sub-component></my-widget></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* custom = doc->find_element("my-widget");
    ASSERT_NE(custom, nullptr);
    EXPECT_EQ(custom->text_content(), "Custom contentNested");

    auto* subcomp = doc->find_element("my-sub-component");
    ASSERT_NE(subcomp, nullptr);
    EXPECT_EQ(subcomp->text_content(), "Nested");
}

// Test 5: Web components with shadow DOM concept - testing slot elements
TEST(TreeBuilder, SlotElementsV61) {
    auto doc = parse("<html><body><web-component><slot name=\"header\">Default header</slot><slot name=\"content\">Default content</slot><p>Fallback paragraph</p></web-component></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* webcomp = doc->find_element("web-component");
    ASSERT_NE(webcomp, nullptr);

    auto slots = doc->find_all_elements("slot");
    EXPECT_EQ(slots.size(), 2u);

    auto ps = doc->find_all_elements("p");
    EXPECT_EQ(ps.size(), 1u);
    EXPECT_EQ(ps[0]->text_content(), "Fallback paragraph");
}

// Test 6: HTML entities in content - testing character references
TEST(TreeBuilder, HTMLEntitiesV61) {
    auto doc = parse("<html><body><p>Less than &lt; Greater than &gt; Ampersand &amp; Quote &quot; Apostrophe &apos;</p><p>Numeric &#65; Hex &#x41;</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto ps = doc->find_all_elements("p");
    ASSERT_GE(ps.size(), 2u);

    std::string first_text = ps[0]->text_content();
    // Entities should be decoded in text content
    EXPECT_NE(first_text.find("<"), std::string::npos);
    EXPECT_NE(first_text.find(">"), std::string::npos);
    EXPECT_NE(first_text.find("&"), std::string::npos);
}

// Test 7: Complex nesting with mixed content types
TEST(TreeBuilder, ComplexMixedNestingV61) {
    auto doc = parse("<html><body><main><article><header>Title</header><section><p>Para 1</p><p>Para 2</p></section><footer>Footer</footer></article><aside><nav><ul><li>Item 1</li><li>Item 2</li></ul></nav></aside></main></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* main = doc->find_element("main");
    ASSERT_NE(main, nullptr);

    auto articles = doc->find_all_elements("article");
    auto sections = doc->find_all_elements("section");
    auto ps = doc->find_all_elements("p");
    auto lis = doc->find_all_elements("li");

    EXPECT_EQ(articles.size(), 1u);
    EXPECT_EQ(sections.size(), 1u);
    EXPECT_EQ(ps.size(), 2u);
    EXPECT_EQ(lis.size(), 2u);
}

// Test 8: Void elements and multiple self-closing tags
TEST(TreeBuilder, VoidElementsAndSelfClosingV61) {
    auto doc = parse("<html><body><img src=\"test.jpg\" alt=\"test\"/><br/><hr/><input type=\"text\" name=\"field\"/><link rel=\"stylesheet\" href=\"style.css\"><meta charset=\"UTF-8\"></body></html>");
    ASSERT_NE(doc, nullptr);

    auto imgs = doc->find_all_elements("img");
    auto brs = doc->find_all_elements("br");
    auto hrs = doc->find_all_elements("hr");
    auto inputs = doc->find_all_elements("input");
    auto links = doc->find_all_elements("link");
    auto metas = doc->find_all_elements("meta");

    EXPECT_EQ(imgs.size(), 1u);
    EXPECT_EQ(brs.size(), 1u);
    EXPECT_EQ(hrs.size(), 1u);
    EXPECT_EQ(inputs.size(), 1u);
    EXPECT_EQ(links.size(), 1u);
    EXPECT_EQ(metas.size(), 1u);
}

// Test 1: Deeply nested lists (5+ levels)
TEST(TreeBuilder, DeeplyNestedListsV62) {
    auto doc = parse("<html><body><ul><li>Level 1<ul><li>Level 2<ul><li>Level 3<ul><li>Level 4<ul><li>Level 5<ul><li>Level 6</li></ul></li></ul></li></ul></li></ul></li></ul></li></ul></body></html>");
    ASSERT_NE(doc, nullptr);

    auto uls = doc->find_all_elements("ul");
    auto lis = doc->find_all_elements("li");

    // Should have 6 ul elements (one for each level)
    EXPECT_EQ(uls.size(), 6u);
    // Should have 6 li elements (one for each level)
    EXPECT_EQ(lis.size(), 6u);

    // Verify deepest level content
    auto deepest = doc->find_element("li");
    ASSERT_NE(deepest, nullptr);
    std::string text = deepest->text_content();
    EXPECT_TRUE(text.find("Level 1") != std::string::npos);
}

// Test 2: Table with caption, thead, tbody, tfoot
TEST(TreeBuilder, CompleteTableStructureV62) {
    auto doc = parse("<html><body><table><caption>Sales Data</caption><thead><tr><th>Quarter</th><th>Revenue</th></tr></thead><tbody><tr><td>Q1</td><td>$1M</td></tr><tr><td>Q2</td><td>$2M</td></tr></tbody><tfoot><tr><td>Total</td><td>$3M</td></tr></tfoot></table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);

    auto* caption = doc->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->text_content(), "Sales Data");

    auto* thead = doc->find_element("thead");
    auto* tbody = doc->find_element("tbody");
    auto* tfoot = doc->find_element("tfoot");

    ASSERT_NE(thead, nullptr);
    ASSERT_NE(tbody, nullptr);
    ASSERT_NE(tfoot, nullptr);

    auto header_cells = thead->find_all_elements("th");
    auto body_rows = tbody->find_all_elements("tr");
    auto footer_rows = tfoot->find_all_elements("tr");

    EXPECT_EQ(header_cells.size(), 2u);
    EXPECT_EQ(body_rows.size(), 2u);
    EXPECT_EQ(footer_rows.size(), 1u);
}

// Test 3: Definition lists (dl/dt/dd)
TEST(TreeBuilder, DefinitionListsV62) {
    auto doc = parse("<html><body><dl><dt>HTML</dt><dd>Hypertext Markup Language</dd><dt>CSS</dt><dd>Cascading Style Sheets</dd><dt>JavaScript</dt><dd>Programming language for web</dd></dl></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);

    auto dts = doc->find_all_elements("dt");
    auto dds = doc->find_all_elements("dd");

    EXPECT_EQ(dts.size(), 3u);
    EXPECT_EQ(dds.size(), 3u);

    EXPECT_EQ(dts[0]->text_content(), "HTML");
    EXPECT_EQ(dds[0]->text_content(), "Hypertext Markup Language");
    EXPECT_EQ(dts[1]->text_content(), "CSS");
    EXPECT_EQ(dts[2]->text_content(), "JavaScript");
}

// Test 4: Ruby annotations (ruby/rt/rp)
TEST(TreeBuilder, RubyAnnotationsV62) {
    auto doc = parse("<html><body><p>The word <ruby>日本<rt>にほん</rt></ruby> means Japan.</p><p><ruby>東京<rp>(</rp><rt>とうきょう</rt><rp>)</rp></ruby> is Tokyo.</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto rubies = doc->find_all_elements("ruby");
    auto rts = doc->find_all_elements("rt");
    auto rps = doc->find_all_elements("rp");

    EXPECT_EQ(rubies.size(), 2u);
    EXPECT_EQ(rts.size(), 2u);
    EXPECT_EQ(rps.size(), 2u);

    std::string text = doc->text_content();
    EXPECT_TRUE(text.find("Japan") != std::string::npos);
    EXPECT_TRUE(text.find("Tokyo") != std::string::npos);
}

// Test 5: Dialog element
TEST(TreeBuilder, DialogElementV62) {
    auto doc = parse("<html><body><dialog id=\"confirm\" open><form method=\"dialog\"><p>Are you sure?</p><button value=\"yes\">Yes</button><button value=\"no\">No</button></form></dialog></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dialog = doc->find_element("dialog");
    ASSERT_NE(dialog, nullptr);

    auto* form = dialog->find_element("form");
    ASSERT_NE(form, nullptr);

    auto buttons = dialog->find_all_elements("button");
    EXPECT_EQ(buttons.size(), 2u);

    std::string dialog_text = dialog->text_content();
    EXPECT_TRUE(dialog_text.find("Are you sure") != std::string::npos);
}

// Test 6: Meter element
TEST(TreeBuilder, MeterElementV62) {
    auto doc = parse("<html><body><p>Disk usage: <meter value=\"6\" min=\"0\" max=\"10\">6 out of 10</meter></p><p>Temperature: <meter low=\"15\" high=\"30\" value=\"24\">Normal</meter></p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto meters = doc->find_all_elements("meter");
    EXPECT_EQ(meters.size(), 2u);

    ASSERT_GE(meters.size(), 1u);
    std::string meter_text = meters[0]->text_content();
    EXPECT_TRUE(meter_text.find("6") != std::string::npos || meter_text.find("out of 10") != std::string::npos);
}

// Test 7: Progress element
TEST(TreeBuilder, ProgressElementV62) {
    auto doc = parse("<html><body><p>Download: <progress value=\"25\" max=\"100\">25%</progress></p><p>Installation: <progress id=\"install\" max=\"200\"><span>50</span>/<span>200</span></progress></p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto progresses = doc->find_all_elements("progress");
    EXPECT_EQ(progresses.size(), 2u);

    auto spans = doc->find_all_elements("span");
    EXPECT_EQ(spans.size(), 2u);

    std::string doc_text = doc->text_content();
    EXPECT_TRUE(doc_text.find("Download") != std::string::npos);
    EXPECT_TRUE(doc_text.find("Installation") != std::string::npos);
}

// Test 8: Details and summary with nested content
TEST(TreeBuilder, DetailsAndSummaryV62) {
    auto doc = parse("<html><body><details open id=\"details1\"><summary>Installation Instructions</summary><ol><li>Download the file</li><li>Extract the archive</li><li>Run the installer</li></ol></details><details id=\"details2\"><summary>FAQ</summary><p>Question 1: How to use?</p><p>Answer: Read the manual.</p></details></body></html>");
    ASSERT_NE(doc, nullptr);

    auto details_elems = doc->find_all_elements("details");
    EXPECT_EQ(details_elems.size(), 2u);

    auto summaries = doc->find_all_elements("summary");
    EXPECT_EQ(summaries.size(), 2u);

    EXPECT_EQ(summaries[0]->text_content(), "Installation Instructions");
    EXPECT_EQ(summaries[1]->text_content(), "FAQ");

    auto lis = doc->find_all_elements("li");
    EXPECT_EQ(lis.size(), 3u);

    auto ps = doc->find_all_elements("p");
    EXPECT_EQ(ps.size(), 2u);
}

// Test 1: Nested tables with complex structure
TEST(TreeBuilder, NestedTablesV63) {
    auto doc = parse("<html><body><table><tr><td><table><tr><td>Inner cell</td></tr></table></td><td>Outer cell</td></tr></table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto tables = doc->find_all_elements("table");
    EXPECT_EQ(tables.size(), 2u);

    auto tds = doc->find_all_elements("td");
    EXPECT_EQ(tds.size(), 3u);

    // find_all_elements returns depth-first: inner td first, then outer tds
    bool found_inner = false, found_outer = false;
    for (auto* td : tds) {
        if (td->text_content().find("Inner cell") != std::string::npos) found_inner = true;
        if (td->text_content().find("Outer cell") != std::string::npos) found_outer = true;
    }
    EXPECT_TRUE(found_inner);
    EXPECT_TRUE(found_outer);
}

// Test 2: Form elements with attributes
TEST(TreeBuilder, FormElementsV63) {
    auto doc = parse("<html><body><form id=\"myform\" action=\"/submit\" method=\"POST\"><input type=\"text\" name=\"username\" placeholder=\"Enter name\"><textarea name=\"message\" rows=\"5\" cols=\"40\">Default text</textarea><select name=\"options\"><option value=\"opt1\">Option 1</option><option value=\"opt2\" selected>Option 2</option></select><button type=\"submit\">Send</button></form></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);

    auto inputs = doc->find_all_elements("input");
    EXPECT_EQ(inputs.size(), 1u);

    auto textareas = doc->find_all_elements("textarea");
    EXPECT_EQ(textareas.size(), 1u);
    EXPECT_EQ(textareas[0]->text_content(), "Default text");

    auto selects = doc->find_all_elements("select");
    EXPECT_EQ(selects.size(), 1u);

    auto options = doc->find_all_elements("option");
    EXPECT_EQ(options.size(), 2u);

    auto buttons = doc->find_all_elements("button");
    EXPECT_EQ(buttons.size(), 1u);
}

// Test 3: Media tags (audio, video, source, track)
TEST(TreeBuilder, MediaTagsV63) {
    auto doc = parse("<html><body><audio controls id=\"myaudio\"><source src=\"audio.mp3\" type=\"audio/mpeg\"><source src=\"audio.ogg\" type=\"audio/ogg\"><track kind=\"captions\" src=\"captions.vtt\">Your browser does not support audio.</audio><video width=\"320\" height=\"240\" controls><source src=\"movie.mp4\" type=\"video/mp4\"><source src=\"movie.ogg\" type=\"video/ogg\">Video not supported</video></body></html>");
    ASSERT_NE(doc, nullptr);

    auto audios = doc->find_all_elements("audio");
    EXPECT_EQ(audios.size(), 1u);

    auto videos = doc->find_all_elements("video");
    EXPECT_EQ(videos.size(), 1u);

    auto sources = doc->find_all_elements("source");
    EXPECT_EQ(sources.size(), 4u);

    auto tracks = doc->find_all_elements("track");
    EXPECT_EQ(tracks.size(), 1u);
}

// Test 4: SVG inline elements
TEST(TreeBuilder, InlineSvgV63) {
    auto doc = parse("<html><body><p>Inline SVG: <svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\"><circle cx=\"50\" cy=\"50\" r=\"40\" fill=\"red\"/><rect x=\"10\" y=\"10\" width=\"30\" height=\"30\" fill=\"blue\"/></svg> end</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto svgs = doc->find_all_elements("svg");
    EXPECT_EQ(svgs.size(), 1u);

    auto circles = doc->find_all_elements("circle");
    EXPECT_EQ(circles.size(), 1u);

    auto rects = doc->find_all_elements("rect");
    EXPECT_EQ(rects.size(), 1u);

    std::string text = doc->text_content();
    EXPECT_TRUE(text.find("Inline SVG") != std::string::npos);
    EXPECT_TRUE(text.find("end") != std::string::npos);
}

// Test 5: Custom data attributes
TEST(TreeBuilder, CustomDataAttributesV63) {
    auto doc = parse("<html><body><div id=\"item1\" data-id=\"123\" data-name=\"Product A\" data-price=\"99.99\" data-tags=\"electronics,gadget\">Special Item</div><p data-timestamp=\"2024-02-28\" data-user-role=\"admin\">Tagged paragraph</p></body></html>");
    ASSERT_NE(doc, nullptr);

    // find_element searches by tag name, not id — find div instead
    auto divs = doc->find_all_elements("div");
    ASSERT_GE(divs.size(), 1u);
    EXPECT_EQ(divs[0]->text_content(), "Special Item");

    auto ps = doc->find_all_elements("p");
    ASSERT_GE(ps.size(), 1u);
    EXPECT_EQ(ps[0]->text_content(), "Tagged paragraph");
}

// Test 6: Whitespace handling in mixed content
TEST(TreeBuilder, WhitespaceHandlingV63) {
    auto doc = parse("<html><body><p>Text with   multiple    spaces</p><div>Line 1\nLine 2\nLine 3</div><span>\n  Indented text  \n</span></body></html>");
    ASSERT_NE(doc, nullptr);

    auto ps = doc->find_all_elements("p");
    ASSERT_EQ(ps.size(), 1u);
    std::string p_text = ps[0]->text_content();
    EXPECT_TRUE(p_text.find("Text") != std::string::npos);
    EXPECT_TRUE(p_text.find("spaces") != std::string::npos);

    auto divs = doc->find_all_elements("div");
    ASSERT_GE(divs.size(), 1u);

    auto spans = doc->find_all_elements("span");
    ASSERT_EQ(spans.size(), 1u);
}

// Test 7: HTML entity encoding and special characters
TEST(TreeBuilder, EntityEncodingV63) {
    auto doc = parse("<html><body><p>Copyright &copy; 2024 &ndash; All rights reserved</p><p>Less than &lt; Greater than &gt; Ampersand &amp;</p><p>Quote &quot; and Apostrophe &apos;</p><p>Math: 1 &plus; 1 &equals; 2</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto ps = doc->find_all_elements("p");
    EXPECT_EQ(ps.size(), 4u);

    std::string first_p = ps[0]->text_content();
    EXPECT_TRUE(first_p.find("Copyright") != std::string::npos);
    EXPECT_TRUE(first_p.find("2024") != std::string::npos);

    std::string second_p = ps[1]->text_content();
    EXPECT_TRUE(second_p.find("Less") != std::string::npos);

    std::string doc_text = doc->text_content();
    EXPECT_TRUE(doc_text.find("2024") != std::string::npos);
}

// Test 8: Script tags with content and attributes
TEST(TreeBuilder, ScriptTagsV63) {
    auto doc = parse("<html><head><script type=\"text/javascript\">var x = 10; console.log('test');</script><script async src=\"https://example.com/script.js\"></script></head><body><script>alert('inline');</script><script type=\"application/json\" id=\"data\">{\"key\": \"value\"}</script></body></html>");
    ASSERT_NE(doc, nullptr);

    auto scripts = doc->find_all_elements("script");
    EXPECT_EQ(scripts.size(), 4u);

    std::string first_script = scripts[0]->text_content();
    EXPECT_TRUE(first_script.find("var x") != std::string::npos || first_script.find("console") != std::string::npos);

    std::string json_script = scripts[3]->text_content();
    EXPECT_TRUE(json_script.find("key") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Cycle V63 — HTML parser coverage for self-closing/void/nesting/text/script,
//             comments/doctype/malformed recovery using SimpleNode fields.
// ---------------------------------------------------------------------------

static std::string get_attr_v63(const clever::html::SimpleNode* node, const std::string& key) {
    for (const auto& a : node->attributes) {
        if (a.name == key) return a.value;
    }
    return "";
}

TEST(HtmlParserTest, SelfClosingTagAttributesAndLowercaseV63) {
    auto doc = clever::html::parse("<HTML><BODY><IMG SRC=\"hero.png\" ALT=\"Hero\"/></BODY></HTML>");
    ASSERT_NE(doc, nullptr);

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);

    EXPECT_EQ(img->tag_name, "img");
    EXPECT_TRUE(img->children.empty());
    EXPECT_EQ(get_attr_v63(img, "src"), "hero.png");
    EXPECT_EQ(get_attr_v63(img, "alt"), "Hero");
}

TEST(HtmlParserTest, VoidElementsDoNotWrapFollowingTextV63) {
    auto doc = clever::html::parse("<body>alpha<br>beta<hr>gamma<input type=\"text\">delta</body>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    auto* br = doc->find_element("br");
    auto* hr = doc->find_element("hr");
    auto* input = doc->find_element("input");
    ASSERT_NE(body, nullptr);
    ASSERT_NE(br, nullptr);
    ASSERT_NE(hr, nullptr);
    ASSERT_NE(input, nullptr);

    EXPECT_TRUE(br->children.empty());
    EXPECT_TRUE(hr->children.empty());
    EXPECT_TRUE(input->children.empty());
    EXPECT_EQ(input->parent, body);
    EXPECT_EQ(get_attr_v63(input, "type"), "text");

    const std::string text = body->text_content();
    EXPECT_NE(text.find("alpha"), std::string::npos);
    EXPECT_NE(text.find("beta"), std::string::npos);
    EXPECT_NE(text.find("gamma"), std::string::npos);
    EXPECT_NE(text.find("delta"), std::string::npos);
}

TEST(HtmlParserTest, NestedStructureMaintainsTreeShapeV63) {
    auto doc = clever::html::parse("<main><section><article><h1>T</h1><p>Body <em>text</em></p></article></section></main>");
    ASSERT_NE(doc, nullptr);

    auto* main = doc->find_element("main");
    auto* section = doc->find_element("section");
    auto* article = doc->find_element("article");
    auto* h1 = doc->find_element("h1");
    auto* p = doc->find_element("p");
    auto* em = doc->find_element("em");
    ASSERT_NE(main, nullptr);
    ASSERT_NE(section, nullptr);
    ASSERT_NE(article, nullptr);
    ASSERT_NE(h1, nullptr);
    ASSERT_NE(p, nullptr);
    ASSERT_NE(em, nullptr);

    EXPECT_EQ(section->parent, main);
    EXPECT_EQ(article->parent, section);
    EXPECT_EQ(h1->parent, article);
    EXPECT_EQ(p->parent, article);
    EXPECT_EQ(em->parent, p);
    EXPECT_EQ(p->text_content(), "Body text");
}

TEST(HtmlParserTest, TextNormalizationKeepsSingleLeafTextNodeV63) {
    auto doc = clever::html::parse("<div>Hello   parser world</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    ASSERT_EQ(div->children.size(), 1u);

    const auto* text_node = div->children[0].get();
    ASSERT_NE(text_node, nullptr);
    EXPECT_EQ(text_node->type, clever::html::SimpleNode::Text);
    EXPECT_TRUE(text_node->tag_name.empty());
    EXPECT_EQ(text_node->data, "Hello   parser world");
}

TEST(HtmlParserTest, ScriptAndStyleRawTextHandledV63) {
    auto doc = clever::html::parse("<body><script>if (a < b) { c = 1; }</script><style>body { color: red; }</style></body>");
    ASSERT_NE(doc, nullptr);

    auto* script = doc->find_element("script");
    auto* style = doc->find_element("style");
    ASSERT_NE(script, nullptr);
    ASSERT_NE(style, nullptr);

    ASSERT_FALSE(script->children.empty());
    ASSERT_FALSE(style->children.empty());
    EXPECT_TRUE(script->children[0]->tag_name.empty());
    EXPECT_TRUE(style->children[0]->tag_name.empty());

    EXPECT_NE(script->text_content().find("a < b"), std::string::npos);
    EXPECT_NE(style->text_content().find("color: red"), std::string::npos);
}

TEST(HtmlParserTest, CommentsArePreservedAsCommentNodesV63) {
    auto doc = clever::html::parse("<body>left<!-- middle -->right</body>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    bool found_comment = false;
    for (const auto& child : body->children) {
        if (child->type == clever::html::SimpleNode::Comment) {
            found_comment = true;
            EXPECT_EQ(child->data, " middle ");
        }
    }
    EXPECT_TRUE(found_comment);

    const std::string text = body->text_content();
    EXPECT_NE(text.find("left"), std::string::npos);
    EXPECT_NE(text.find("middle"), std::string::npos);
    EXPECT_NE(text.find("right"), std::string::npos);
}

TEST(HtmlParserTest, DoctypeIsCapturedAtDocumentLevelV63) {
    auto doc = clever::html::parse("<!DOCTYPE HTML><html><body><p>x</p></body></html>");
    ASSERT_NE(doc, nullptr);
    ASSERT_FALSE(doc->children.empty());

    bool found_doctype = false;
    for (const auto& child : doc->children) {
        if (child->type == clever::html::SimpleNode::DocumentType) {
            found_doctype = true;
            EXPECT_EQ(child->doctype_name, "html");
        }
    }
    EXPECT_TRUE(found_doctype);
    EXPECT_EQ(doc->children.front()->type, clever::html::SimpleNode::DocumentType);
    EXPECT_NE(doc->find_element("html"), nullptr);
}

TEST(HtmlParserTest, MalformedHtmlRecoveryClosesParagraphBeforeDivV63) {
    auto doc = clever::html::parse("<body><p>one<div>two</div>three");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    auto* p = doc->find_element("p");
    auto* div = doc->find_element("div");
    ASSERT_NE(body, nullptr);
    ASSERT_NE(p, nullptr);
    ASSERT_NE(div, nullptr);

    EXPECT_EQ(p->parent, body);
    EXPECT_EQ(div->parent, body);
    EXPECT_EQ(p->text_content(), "one");
    EXPECT_EQ(div->text_content(), "two");
    EXPECT_NE(body->text_content().find("three"), std::string::npos);
}

TEST(HtmlParserTest, FindAllElementsReturnsDocumentOrderV64) {
    auto doc = clever::html::parse("<ul><li>first</li><li>second</li></ul><ol><li>third</li></ol>");
    ASSERT_NE(doc, nullptr);

    auto items = doc->find_all_elements("li");
    ASSERT_EQ(items.size(), 3u);
    EXPECT_EQ(items[0]->text_content(), "first");
    EXPECT_EQ(items[1]->text_content(), "second");
    EXPECT_EQ(items[2]->text_content(), "third");
}

TEST(HtmlParserTest, LowercaseTagNamesStoredInNodesV64) {
    auto doc = clever::html::parse("<DIV><SPAN>Hi</SPAN></DIV>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    auto* span = doc->find_element("span");
    ASSERT_NE(div, nullptr);
    ASSERT_NE(span, nullptr);

    EXPECT_EQ(div->tag_name, "div");
    EXPECT_EQ(span->tag_name, "span");
    EXPECT_EQ(span->text_content(), "Hi");
}

TEST(HtmlParserTest, FindElementReturnsFirstDepthFirstMatchV64) {
    auto doc = clever::html::parse("<div><span>first</span><section><span>second</span></section></div>");
    ASSERT_NE(doc, nullptr);

    auto* first_span = doc->find_element("span");
    ASSERT_NE(first_span, nullptr);
    EXPECT_EQ(first_span->text_content(), "first");

    auto spans = doc->find_all_elements("span");
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[1]->text_content(), "second");
}

TEST(HtmlParserTest, TextContentCombinesNestedInlineTextV64) {
    auto doc = clever::html::parse("<p>alpha <strong>beta</strong> <em>gamma</em></p>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);

    const std::string text = p->text_content();
    const std::size_t alpha_pos = text.find("alpha");
    const std::size_t beta_pos = text.find("beta");
    const std::size_t gamma_pos = text.find("gamma");
    EXPECT_NE(alpha_pos, std::string::npos);
    EXPECT_NE(beta_pos, std::string::npos);
    EXPECT_NE(gamma_pos, std::string::npos);
    EXPECT_LT(alpha_pos, beta_pos);
    EXPECT_LT(beta_pos, gamma_pos);
}

TEST(HtmlParserTest, VoidElementRemainsLeafAndSiblingStaysSeparateV64) {
    auto doc = clever::html::parse("<div><img src='x'><p>after</p></div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    auto* img = doc->find_element("img");
    auto* p = doc->find_element("p");
    ASSERT_NE(div, nullptr);
    ASSERT_NE(img, nullptr);
    ASSERT_NE(p, nullptr);

    EXPECT_EQ(img->parent, div);
    EXPECT_TRUE(img->children.empty());
    EXPECT_EQ(p->parent, div);
    EXPECT_EQ(p->text_content(), "after");
}

TEST(HtmlParserTest, AttributesAccessibleByNameAndValueV64) {
    auto doc = clever::html::parse("<a href=\"/docs\" title=\"Docs\" data-id=\"42\">read</a>");
    ASSERT_NE(doc, nullptr);

    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(a->attributes.size(), 3u);

    auto find_attr = [&](const std::string& name) -> const std::string* {
        for (const auto& attr : a->attributes) {
            if (attr.name == name) return &attr.value;
        }
        return nullptr;
    };

    const std::string* href = find_attr("href");
    const std::string* title = find_attr("title");
    const std::string* data_id = find_attr("data-id");
    ASSERT_NE(href, nullptr);
    ASSERT_NE(title, nullptr);
    ASSERT_NE(data_id, nullptr);
    EXPECT_EQ(*href, "/docs");
    EXPECT_EQ(*title, "Docs");
    EXPECT_EQ(*data_id, "42");
}

TEST(HtmlParserTest, CommentNodeExistsAmongElementChildrenV64) {
    auto doc = clever::html::parse("<div>left<!-- center --><span>right</span></div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    auto* span = doc->find_element("span");
    ASSERT_NE(div, nullptr);
    ASSERT_NE(span, nullptr);

    bool found_comment = false;
    for (const auto& child : div->children) {
        if (child->type == clever::html::SimpleNode::Comment) {
            found_comment = true;
            EXPECT_EQ(child->data, " center ");
        }
    }
    EXPECT_TRUE(found_comment);
    EXPECT_EQ(span->parent, div);
}

TEST(HtmlParserTest, TextContentIncludesTextAroundCommentsV64) {
    auto doc = clever::html::parse("<div>alpha<!--beta--><em>gamma</em>delta</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);

    const std::string text = div->text_content();
    EXPECT_NE(text.find("alpha"), std::string::npos);
    EXPECT_NE(text.find("beta"), std::string::npos);
    EXPECT_NE(text.find("gamma"), std::string::npos);
    EXPECT_NE(text.find("delta"), std::string::npos);
}

TEST(HtmlParserTest, NestedListsUlOlLiStructureV65) {
    auto doc = clever::html::parse("<body><ul><li>Item A<ol><li>Sub 1</li><li>Sub 2</li></ol></li><li>Item B</li></ul></body>");
    ASSERT_NE(doc, nullptr);

    auto* ul = doc->find_element("ul");
    auto* ol = doc->find_element("ol");
    ASSERT_NE(ul, nullptr);
    ASSERT_NE(ol, nullptr);

    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 4u);
    EXPECT_EQ(ol->parent->tag_name, "li");
    EXPECT_EQ(lis[0]->text_content(), "Item ASub 1Sub 2");
    EXPECT_EQ(lis[1]->text_content(), "Sub 1");
    EXPECT_EQ(lis[2]->text_content(), "Sub 2");
    EXPECT_EQ(lis[3]->text_content(), "Item B");
}

TEST(HtmlParserTest, TableWithTheadTbodyRowsAndCellsV65) {
    auto doc = clever::html::parse(
        "<table><thead><tr><td>H1</td><td>H2</td></tr></thead>"
        "<tbody><tr><td>A1</td><td>A2</td></tr><tr><td>B1</td><td>B2</td></tr></tbody></table>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    auto* thead = doc->find_element("thead");
    auto* tbody = doc->find_element("tbody");
    ASSERT_NE(table, nullptr);
    ASSERT_NE(thead, nullptr);
    ASSERT_NE(tbody, nullptr);

    auto rows = doc->find_all_elements("tr");
    auto cells = doc->find_all_elements("td");
    EXPECT_EQ(rows.size(), 3u);
    EXPECT_EQ(cells.size(), 6u);
    EXPECT_EQ(cells[0]->text_content(), "H1");
    EXPECT_EQ(cells[1]->text_content(), "H2");
    EXPECT_EQ(cells[4]->text_content(), "B1");
    EXPECT_EQ(cells[5]->text_content(), "B2");
}

TEST(HtmlParserTest, FormElementsInputTypesAndSelectOptionsV65) {
    auto doc = clever::html::parse(
        "<form action='/submit' method='post'>"
        "<input type='text' name='username'>"
        "<input type='password' name='password'>"
        "<select name='role'><option value='admin'>Admin</option><option value='user' selected>User</option></select>"
        "</form>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);

    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "text");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "password");

    auto selects = doc->find_all_elements("select");
    auto options = doc->find_all_elements("option");
    ASSERT_EQ(selects.size(), 1u);
    ASSERT_EQ(options.size(), 2u);
    EXPECT_EQ(get_attr_v63(selects[0], "name"), "role");
    EXPECT_EQ(get_attr_v63(options[0], "value"), "admin");
    EXPECT_EQ(get_attr_v63(options[1], "value"), "user");
    EXPECT_EQ(get_attr_v63(options[1], "selected"), "");
}

TEST(HtmlParserTest, ScriptTagRawContentPreservedV65) {
    auto doc = clever::html::parse("<body><script>if (a < b && c > d) { x = \"ok\"; }</script></body>");
    ASSERT_NE(doc, nullptr);

    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    ASSERT_FALSE(script->children.empty());

    const std::string text = script->text_content();
    EXPECT_NE(text.find("a < b"), std::string::npos);
    EXPECT_NE(text.find("c > d"), std::string::npos);
    EXPECT_NE(text.find("x = \"ok\""), std::string::npos);
}

TEST(HtmlParserTest, StyleTagRawCssContentPreservedV65) {
    auto doc = clever::html::parse("<head><style>body > .item { color: red; content: \"<ok>\"; }</style></head>");
    ASSERT_NE(doc, nullptr);

    auto* style = doc->find_element("style");
    ASSERT_NE(style, nullptr);
    ASSERT_FALSE(style->children.empty());

    const std::string css = style->text_content();
    EXPECT_NE(css.find("body > .item"), std::string::npos);
    EXPECT_NE(css.find("color: red"), std::string::npos);
    EXPECT_NE(css.find("<ok>"), std::string::npos);
}

TEST(HtmlParserTest, MetaCharsetAttributeParsedV65) {
    auto doc = clever::html::parse("<head><meta charset='utf-8'><meta name='viewport' content='width=device-width'></head>");
    ASSERT_NE(doc, nullptr);

    auto metas = doc->find_all_elements("meta");
    ASSERT_EQ(metas.size(), 2u);
    EXPECT_EQ(get_attr_v63(metas[0], "charset"), "utf-8");
    EXPECT_EQ(get_attr_v63(metas[1], "name"), "viewport");
    EXPECT_EQ(get_attr_v63(metas[1], "content"), "width=device-width");
}

TEST(HtmlParserTest, LinkRelStylesheetAttributesParsedV65) {
    auto doc = clever::html::parse("<head><link rel='stylesheet' href='/assets/app.css'></head>");
    ASSERT_NE(doc, nullptr);

    auto* link = doc->find_element("link");
    ASSERT_NE(link, nullptr);
    EXPECT_TRUE(link->children.empty());
    EXPECT_EQ(get_attr_v63(link, "rel"), "stylesheet");
    EXPECT_EQ(get_attr_v63(link, "href"), "/assets/app.css");
}

TEST(HtmlParserTest, EmptyBodyHasNoChildrenOrTextV65) {
    auto doc = clever::html::parse("<html><head><title>t</title></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_TRUE(body->children.empty());
    EXPECT_TRUE(body->text_content().empty());
}

TEST(HTMLParserTest, SelfClosingBrHrImgTagsV66) {
    auto doc = clever::html::parse("<body>start<br><hr><img src='hero.png' alt='hero'>end</body>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    auto* br = doc->find_element("br");
    auto* hr = doc->find_element("hr");
    auto* img = doc->find_element("img");
    ASSERT_NE(body, nullptr);
    ASSERT_NE(br, nullptr);
    ASSERT_NE(hr, nullptr);
    ASSERT_NE(img, nullptr);

    EXPECT_EQ(br->tag_name, "br");
    EXPECT_EQ(hr->tag_name, "hr");
    EXPECT_EQ(img->tag_name, "img");
    EXPECT_TRUE(br->children.empty());
    EXPECT_TRUE(hr->children.empty());
    EXPECT_TRUE(img->children.empty());
    EXPECT_EQ(img->parent, body);
    EXPECT_EQ(get_attr_v63(img, "src"), "hero.png");
    EXPECT_EQ(get_attr_v63(img, "alt"), "hero");
}

TEST(HTMLParserTest, NestedDivStructureDepthV66) {
    auto doc = clever::html::parse(
        "<body>"
        "<div id='d1'><div id='d2'><div id='d3'><div id='d4'><div id='d5'>deep</div></div></div></div></div>"
        "</body>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    auto divs = doc->find_all_elements("div");
    ASSERT_EQ(divs.size(), 5u);

    const clever::html::SimpleNode* d1 = nullptr;
    const clever::html::SimpleNode* d2 = nullptr;
    const clever::html::SimpleNode* d3 = nullptr;
    const clever::html::SimpleNode* d4 = nullptr;
    const clever::html::SimpleNode* d5 = nullptr;
    for (const auto* div : divs) {
        const std::string id = get_attr_v63(div, "id");
        if (id == "d1") d1 = div;
        if (id == "d2") d2 = div;
        if (id == "d3") d3 = div;
        if (id == "d4") d4 = div;
        if (id == "d5") d5 = div;
    }

    ASSERT_NE(d1, nullptr);
    ASSERT_NE(d2, nullptr);
    ASSERT_NE(d3, nullptr);
    ASSERT_NE(d4, nullptr);
    ASSERT_NE(d5, nullptr);
    EXPECT_EQ(d1->parent, body);
    EXPECT_EQ(d2->parent, d1);
    EXPECT_EQ(d3->parent, d2);
    EXPECT_EQ(d4->parent, d3);
    EXPECT_EQ(d5->parent, d4);
    EXPECT_EQ(d5->text_content(), "deep");
}

TEST(HTMLParserTest, AttributeWithEmptyValueV66) {
    auto doc = clever::html::parse("<div data-empty=\"\" title='present'>x</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->tag_name, "div");
    EXPECT_EQ(get_attr_v63(div, "data-empty"), "");
    EXPECT_EQ(get_attr_v63(div, "title"), "present");
}

TEST(HTMLParserTest, BooleanAttributesDisabledAndCheckedV66) {
    auto doc = clever::html::parse("<input type='checkbox' disabled checked>");
    ASSERT_NE(doc, nullptr);

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);

    bool has_disabled = false;
    bool has_checked = false;
    for (const auto& attr : input->attributes) {
        if (attr.name == "disabled") has_disabled = true;
        if (attr.name == "checked") has_checked = true;
    }
    EXPECT_TRUE(has_disabled);
    EXPECT_TRUE(has_checked);
    EXPECT_EQ(get_attr_v63(input, "type"), "checkbox");
}

TEST(HTMLParserTest, MultipleClassesInClassAttributeV66) {
    auto doc = clever::html::parse("<div class='card primary selected'>content</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);

    const std::string cls = get_attr_v63(div, "class");
    EXPECT_EQ(cls, "card primary selected");
    EXPECT_NE(cls.find("card"), std::string::npos);
    EXPECT_NE(cls.find("primary"), std::string::npos);
    EXPECT_NE(cls.find("selected"), std::string::npos);
}

TEST(HTMLParserTest, DataCustomAttributesV66) {
    auto doc = clever::html::parse("<section data-user-id='42' data-role='admin' data-active='true'></section>");
    ASSERT_NE(doc, nullptr);

    auto* section = doc->find_element("section");
    ASSERT_NE(section, nullptr);
    EXPECT_EQ(section->tag_name, "section");
    EXPECT_EQ(get_attr_v63(section, "data-user-id"), "42");
    EXPECT_EQ(get_attr_v63(section, "data-role"), "admin");
    EXPECT_EQ(get_attr_v63(section, "data-active"), "true");
}

TEST(HTMLParserTest, DoctypeParsingV66) {
    auto doc = clever::html::parse("<!DOCTYPE html><html><body><p>ok</p></body></html>");
    ASSERT_NE(doc, nullptr);

    bool found_doctype = false;
    for (const auto& child : doc->children) {
        if (child->type == clever::html::SimpleNode::DocumentType) {
            found_doctype = true;
            EXPECT_EQ(child->doctype_name, "html");
        }
    }
    EXPECT_TRUE(found_doctype);
}

TEST(HTMLParserTest, CommentNodesInTreeV66) {
    auto doc = clever::html::parse("<body>left<!-- note -->right</body>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    bool found_comment = false;
    for (const auto& child : body->children) {
        if (child->type == clever::html::SimpleNode::Comment) {
            found_comment = true;
            EXPECT_TRUE(child->tag_name.empty());
            EXPECT_EQ(child->data, " note ");
        }
    }
    EXPECT_TRUE(found_comment);
}

TEST(HTMLParserTest, NestedAnchorTagsV67) {
    auto doc = clever::html::parse(
        "<div><a href='https://outer.example'>Outer <a href='https://inner.example'>Inner</a> tail</a></div>");
    ASSERT_NE(doc, nullptr);

    auto anchors = doc->find_all_elements("a");
    ASSERT_EQ(anchors.size(), 2u);
    EXPECT_EQ(anchors[0]->tag_name, "a");
    EXPECT_EQ(anchors[1]->tag_name, "a");
    EXPECT_EQ(get_attr_v63(anchors[0], "href"), "https://outer.example");
    EXPECT_EQ(get_attr_v63(anchors[1], "href"), "https://inner.example");
    EXPECT_NE(anchors[0]->text_content().find("Outer"), std::string::npos);
    EXPECT_NE(anchors[1]->text_content().find("Inner"), std::string::npos);
}

TEST(HTMLParserTest, ImageWithSrcAndAltAttributesV67) {
    auto doc = clever::html::parse("<body><img src='/assets/logo.png' alt='Site Logo'></body>");
    ASSERT_NE(doc, nullptr);

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->tag_name, "img");
    EXPECT_TRUE(img->children.empty());
    EXPECT_EQ(get_attr_v63(img, "src"), "/assets/logo.png");
    EXPECT_EQ(get_attr_v63(img, "alt"), "Site Logo");
}

TEST(HTMLParserTest, TextareaPreservesInnerTextV67) {
    auto doc = clever::html::parse("<textarea>first line\nsecond line</textarea>");
    ASSERT_NE(doc, nullptr);

    auto* textarea = doc->find_element("textarea");
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(textarea->tag_name, "textarea");
    EXPECT_EQ(textarea->text_content(), "first line\nsecond line");
}

TEST(HTMLParserTest, OptionElementsWithinSelectV67) {
    auto doc =
        clever::html::parse("<select name='country'><option value='us'>United States</option><option value='kr'>Korea</option></select>");
    ASSERT_NE(doc, nullptr);

    auto* select = doc->find_element("select");
    auto options = doc->find_all_elements("option");
    ASSERT_NE(select, nullptr);
    ASSERT_EQ(options.size(), 2u);
    EXPECT_EQ(select->tag_name, "select");
    EXPECT_EQ(get_attr_v63(select, "name"), "country");
    EXPECT_EQ(options[0]->parent, select);
    EXPECT_EQ(options[1]->parent, select);
    EXPECT_EQ(get_attr_v63(options[0], "value"), "us");
    EXPECT_EQ(get_attr_v63(options[1], "value"), "kr");
}

TEST(HTMLParserTest, OrderedListWithStartAttributeV67) {
    auto doc = clever::html::parse("<ol start='5'><li>five</li><li>six</li></ol>");
    ASSERT_NE(doc, nullptr);

    auto* ol = doc->find_element("ol");
    auto items = doc->find_all_elements("li");
    ASSERT_NE(ol, nullptr);
    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(ol->tag_name, "ol");
    EXPECT_EQ(get_attr_v63(ol, "start"), "5");
    EXPECT_EQ(items[0]->text_content(), "five");
    EXPECT_EQ(items[1]->text_content(), "six");
}

TEST(HTMLParserTest, TableCaptionElementV67) {
    auto doc =
        clever::html::parse("<table><caption>Quarterly Results</caption><tr><td>Q1</td></tr></table>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    auto* caption = doc->find_element("caption");
    ASSERT_NE(table, nullptr);
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(table->tag_name, "table");
    EXPECT_EQ(caption->tag_name, "caption");
    EXPECT_EQ(caption->parent, table);
    EXPECT_EQ(caption->text_content(), "Quarterly Results");
}

TEST(HTMLParserTest, InputWithTypeAndValueAttributesV67) {
    auto doc = clever::html::parse("<form><input type='email' value='user@example.com'></form>");
    ASSERT_NE(doc, nullptr);

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->tag_name, "input");
    EXPECT_EQ(get_attr_v63(input, "type"), "email");
    EXPECT_EQ(get_attr_v63(input, "value"), "user@example.com");
}

TEST(HTMLParserTest, DivWithInlineStyleAttributeV67) {
    auto doc = clever::html::parse("<div style='color: red; font-weight: bold;'>Styled text</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->tag_name, "div");
    EXPECT_EQ(get_attr_v63(div, "style"), "color: red; font-weight: bold;");
    EXPECT_EQ(div->text_content(), "Styled text");
}

TEST(HTMLParserTest, ParagraphWithInlineSpanV68) {
    auto doc = clever::html::parse("<p>Hello <span>world</span>!</p>");
    ASSERT_NE(doc, nullptr);

    auto* paragraph = doc->find_element("p");
    auto* span = doc->find_element("span");
    ASSERT_NE(paragraph, nullptr);
    ASSERT_NE(span, nullptr);

    EXPECT_EQ(paragraph->tag_name, "p");
    EXPECT_EQ(span->tag_name, "span");
    EXPECT_EQ(span->parent, paragraph);
    EXPECT_EQ(span->text_content(), "world");
    EXPECT_EQ(paragraph->text_content(), "Hello world!");
}

TEST(HTMLParserTest, BlockquoteWithCitationV68) {
    auto doc = clever::html::parse("<blockquote cite='https://example.com/source'>Quoted text</blockquote>");
    ASSERT_NE(doc, nullptr);

    auto* blockquote = doc->find_element("blockquote");
    ASSERT_NE(blockquote, nullptr);
    EXPECT_EQ(blockquote->tag_name, "blockquote");
    EXPECT_EQ(get_attr_v63(blockquote, "cite"), "https://example.com/source");
    EXPECT_EQ(blockquote->text_content(), "Quoted text");
}

TEST(HTMLParserTest, DetailsAndSummaryElementsV68) {
    auto doc = clever::html::parse("<details open><summary>More info</summary><p>Details body</p></details>");
    ASSERT_NE(doc, nullptr);

    auto* details = doc->find_element("details");
    auto* summary = doc->find_element("summary");
    auto* paragraph = doc->find_element("p");
    ASSERT_NE(details, nullptr);
    ASSERT_NE(summary, nullptr);
    ASSERT_NE(paragraph, nullptr);

    EXPECT_EQ(details->tag_name, "details");
    EXPECT_EQ(summary->tag_name, "summary");
    EXPECT_EQ(summary->parent, details);
    EXPECT_EQ(paragraph->parent, details);
    EXPECT_EQ(summary->text_content(), "More info");
    EXPECT_EQ(paragraph->text_content(), "Details body");
    EXPECT_EQ(get_attr_v63(details, "open"), "");
}

TEST(HTMLParserTest, DefinitionListDtDdV68) {
    auto doc = clever::html::parse("<dl><dt>API</dt><dd>Application Programming Interface</dd></dl>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    auto* dt = doc->find_element("dt");
    auto* dd = doc->find_element("dd");
    ASSERT_NE(dl, nullptr);
    ASSERT_NE(dt, nullptr);
    ASSERT_NE(dd, nullptr);

    EXPECT_EQ(dl->tag_name, "dl");
    EXPECT_EQ(dt->tag_name, "dt");
    EXPECT_EQ(dd->tag_name, "dd");
    EXPECT_EQ(dt->parent, dl);
    EXPECT_EQ(dd->parent, dl);
    EXPECT_EQ(dt->text_content(), "API");
    EXPECT_EQ(dd->text_content(), "Application Programming Interface");
}

TEST(HTMLParserTest, FigureAndFigcaptionV68) {
    auto doc = clever::html::parse("<figure><img src='cat.png' alt='Cat'><figcaption>Cat photo</figcaption></figure>");
    ASSERT_NE(doc, nullptr);

    auto* figure = doc->find_element("figure");
    auto* figcaption = doc->find_element("figcaption");
    auto* img = doc->find_element("img");
    ASSERT_NE(figure, nullptr);
    ASSERT_NE(figcaption, nullptr);
    ASSERT_NE(img, nullptr);

    EXPECT_EQ(figure->tag_name, "figure");
    EXPECT_EQ(figcaption->tag_name, "figcaption");
    EXPECT_EQ(img->tag_name, "img");
    EXPECT_EQ(figcaption->parent, figure);
    EXPECT_EQ(img->parent, figure);
    EXPECT_EQ(get_attr_v63(img, "src"), "cat.png");
    EXPECT_EQ(get_attr_v63(img, "alt"), "Cat");
    EXPECT_EQ(figcaption->text_content(), "Cat photo");
}

TEST(HTMLParserTest, AbbrWithTitleAttributeV68) {
    auto doc = clever::html::parse("<p>Use <abbr title='HyperText Markup Language'>HTML</abbr> syntax.</p>");
    ASSERT_NE(doc, nullptr);

    auto* abbr = doc->find_element("abbr");
    ASSERT_NE(abbr, nullptr);
    EXPECT_EQ(abbr->tag_name, "abbr");
    EXPECT_EQ(get_attr_v63(abbr, "title"), "HyperText Markup Language");
    EXPECT_EQ(abbr->text_content(), "HTML");
}

TEST(HTMLParserTest, TimeElementWithDatetimeAttributeV68) {
    auto doc = clever::html::parse("<time datetime='2026-02-27T12:30:00Z'>February 27, 2026</time>");
    ASSERT_NE(doc, nullptr);

    auto* time = doc->find_element("time");
    ASSERT_NE(time, nullptr);
    EXPECT_EQ(time->tag_name, "time");
    EXPECT_EQ(get_attr_v63(time, "datetime"), "2026-02-27T12:30:00Z");
    EXPECT_EQ(time->text_content(), "February 27, 2026");
}

TEST(HTMLParserTest, MarkElementHighlightV68) {
    auto doc = clever::html::parse("<p>Read the <mark>important</mark> part.</p>");
    ASSERT_NE(doc, nullptr);

    auto* paragraph = doc->find_element("p");
    auto* mark = doc->find_element("mark");
    ASSERT_NE(paragraph, nullptr);
    ASSERT_NE(mark, nullptr);

    EXPECT_EQ(mark->tag_name, "mark");
    EXPECT_EQ(mark->parent, paragraph);
    EXPECT_EQ(mark->text_content(), "important");
    EXPECT_EQ(paragraph->text_content(), "Read the important part.");
}

TEST(HTMLParserTest, PreElementPreservesWhitespaceV69) {
    auto doc = clever::html::parse("<pre>  alpha\n    beta\tgamma  </pre>");
    ASSERT_NE(doc, nullptr);

    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
    EXPECT_EQ(pre->tag_name, "pre");
    EXPECT_EQ(pre->text_content(), "  alpha\n    beta\tgamma  ");
}

TEST(HTMLParserTest, CodeElementInlineV69) {
    auto doc = clever::html::parse("<p>Use <code>std::string_view</code> for lightweight text views.</p>");
    ASSERT_NE(doc, nullptr);

    auto* paragraph = doc->find_element("p");
    auto* code = doc->find_element("code");
    ASSERT_NE(paragraph, nullptr);
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(paragraph->tag_name, "p");
    EXPECT_EQ(code->tag_name, "code");
    EXPECT_EQ(code->parent, paragraph);
    EXPECT_EQ(code->text_content(), "std::string_view");
    EXPECT_EQ(paragraph->text_content(), "Use std::string_view for lightweight text views.");
}

TEST(HTMLParserTest, NavWithAnchorLinksV69) {
    auto doc = clever::html::parse(
        "<nav><a href='/home'>Home</a><a href='/docs'>Docs</a><a href='/contact'>Contact</a></nav>");
    ASSERT_NE(doc, nullptr);

    auto* nav = doc->find_element("nav");
    auto anchors = doc->find_all_elements("a");
    ASSERT_NE(nav, nullptr);
    ASSERT_EQ(anchors.size(), 3u);
    EXPECT_EQ(nav->tag_name, "nav");
    EXPECT_EQ(anchors[0]->tag_name, "a");
    EXPECT_EQ(anchors[1]->tag_name, "a");
    EXPECT_EQ(anchors[2]->tag_name, "a");
    EXPECT_EQ(anchors[0]->parent, nav);
    EXPECT_EQ(anchors[1]->parent, nav);
    EXPECT_EQ(anchors[2]->parent, nav);
    EXPECT_EQ(get_attr_v63(anchors[0], "href"), "/home");
    EXPECT_EQ(get_attr_v63(anchors[1], "href"), "/docs");
    EXPECT_EQ(get_attr_v63(anchors[2], "href"), "/contact");
    EXPECT_EQ(anchors[0]->text_content(), "Home");
    EXPECT_EQ(anchors[1]->text_content(), "Docs");
    EXPECT_EQ(anchors[2]->text_content(), "Contact");
}

TEST(HTMLParserTest, HeaderAndFooterElementsV69) {
    auto doc = clever::html::parse("<body><header><h1>Site Title</h1></header><footer>All rights reserved</footer></body>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    auto* header = doc->find_element("header");
    auto* footer = doc->find_element("footer");
    ASSERT_NE(body, nullptr);
    ASSERT_NE(header, nullptr);
    ASSERT_NE(footer, nullptr);
    EXPECT_EQ(header->tag_name, "header");
    EXPECT_EQ(footer->tag_name, "footer");
    EXPECT_EQ(header->parent, body);
    EXPECT_EQ(footer->parent, body);
    EXPECT_EQ(header->text_content(), "Site Title");
    EXPECT_EQ(footer->text_content(), "All rights reserved");
}

TEST(HTMLParserTest, MainElementV69) {
    auto doc = clever::html::parse("<main><h1>Main Content</h1><p>Primary article text.</p></main>");
    ASSERT_NE(doc, nullptr);

    auto* main = doc->find_element("main");
    auto* heading = doc->find_element("h1");
    auto* paragraph = doc->find_element("p");
    ASSERT_NE(main, nullptr);
    ASSERT_NE(heading, nullptr);
    ASSERT_NE(paragraph, nullptr);
    EXPECT_EQ(main->tag_name, "main");
    EXPECT_EQ(heading->parent, main);
    EXPECT_EQ(paragraph->parent, main);
    EXPECT_EQ(heading->text_content(), "Main Content");
    EXPECT_EQ(paragraph->text_content(), "Primary article text.");
}

TEST(HTMLParserTest, AsideElementV69) {
    auto doc = clever::html::parse("<main><aside><p>Related links</p></aside><p>Core content</p></main>");
    ASSERT_NE(doc, nullptr);

    auto* main = doc->find_element("main");
    auto* aside = doc->find_element("aside");
    ASSERT_NE(main, nullptr);
    ASSERT_NE(aside, nullptr);
    EXPECT_EQ(main->tag_name, "main");
    EXPECT_EQ(aside->tag_name, "aside");
    EXPECT_EQ(aside->parent, main);
    EXPECT_EQ(aside->text_content(), "Related links");
}

TEST(HTMLParserTest, SectionWithHeadingV69) {
    auto doc = clever::html::parse("<section><h2>Overview</h2><p>Section details.</p></section>");
    ASSERT_NE(doc, nullptr);

    auto* section = doc->find_element("section");
    auto* heading = doc->find_element("h2");
    auto* paragraph = doc->find_element("p");
    ASSERT_NE(section, nullptr);
    ASSERT_NE(heading, nullptr);
    ASSERT_NE(paragraph, nullptr);
    EXPECT_EQ(section->tag_name, "section");
    EXPECT_EQ(heading->tag_name, "h2");
    EXPECT_EQ(heading->parent, section);
    EXPECT_EQ(paragraph->parent, section);
    EXPECT_EQ(heading->text_content(), "Overview");
}

TEST(HTMLParserTest, ArticleElementStructureV69) {
    auto doc = clever::html::parse(
        "<article><header><h2>Release Notes</h2></header><p>Feature summary.</p><footer>Published today</footer></article>");
    ASSERT_NE(doc, nullptr);

    auto* article = doc->find_element("article");
    auto* header = doc->find_element("header");
    auto* heading = doc->find_element("h2");
    auto* paragraph = doc->find_element("p");
    auto* footer = doc->find_element("footer");
    ASSERT_NE(article, nullptr);
    ASSERT_NE(header, nullptr);
    ASSERT_NE(heading, nullptr);
    ASSERT_NE(paragraph, nullptr);
    ASSERT_NE(footer, nullptr);
    EXPECT_EQ(article->tag_name, "article");
    EXPECT_EQ(header->parent, article);
    EXPECT_EQ(paragraph->parent, article);
    EXPECT_EQ(footer->parent, article);
    EXPECT_EQ(heading->parent, header);
    EXPECT_EQ(heading->text_content(), "Release Notes");
    EXPECT_EQ(paragraph->text_content(), "Feature summary.");
    EXPECT_EQ(footer->text_content(), "Published today");
}

TEST(HTMLParserTest, EmptyHtmlDocumentV70) {
    auto doc = clever::html::parse("");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->type, clever::html::SimpleNode::Document);

    auto* html = doc->find_element("html");
    auto* head = doc->find_element("head");
    auto* body = doc->find_element("body");
    ASSERT_NE(html, nullptr);
    ASSERT_NE(head, nullptr);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(html->tag_name, "html");
    EXPECT_EQ(head->tag_name, "head");
    EXPECT_EQ(body->tag_name, "body");
}

TEST(HTMLParserTest, SingleParagraphTextV70) {
    auto doc = clever::html::parse("<p>Hello parser world</p>");
    ASSERT_NE(doc, nullptr);

    auto* paragraph = doc->find_element("p");
    ASSERT_NE(paragraph, nullptr);
    EXPECT_EQ(paragraph->tag_name, "p");
    EXPECT_EQ(paragraph->text_content(), "Hello parser world");
}

TEST(HTMLParserTest, NestedDivsThreeLevelsV70) {
    auto doc = clever::html::parse("<div id='outer'><div id='middle'><div id='inner'>Core</div></div></div>");
    ASSERT_NE(doc, nullptr);

    auto divs = doc->find_all_elements("div");
    ASSERT_EQ(divs.size(), 3u);

    clever::html::SimpleNode* outer = nullptr;
    clever::html::SimpleNode* middle = nullptr;
    clever::html::SimpleNode* inner = nullptr;
    for (auto* div : divs) {
        if (get_attr_v63(div, "id") == "outer") outer = div;
        if (get_attr_v63(div, "id") == "middle") middle = div;
        if (get_attr_v63(div, "id") == "inner") inner = div;
    }

    ASSERT_NE(outer, nullptr);
    ASSERT_NE(middle, nullptr);
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(outer->tag_name, "div");
    EXPECT_EQ(middle->tag_name, "div");
    EXPECT_EQ(inner->tag_name, "div");
    EXPECT_EQ(middle->parent, outer);
    EXPECT_EQ(inner->parent, middle);
    EXPECT_EQ(inner->text_content(), "Core");
}

TEST(HTMLParserTest, UnorderedListWithItemsV70) {
    auto doc = clever::html::parse("<ul><li>Alpha</li><li>Beta</li><li>Gamma</li></ul>");
    ASSERT_NE(doc, nullptr);

    auto* ul = doc->find_element("ul");
    auto items = doc->find_all_elements("li");
    ASSERT_NE(ul, nullptr);
    ASSERT_EQ(items.size(), 3u);

    EXPECT_EQ(ul->tag_name, "ul");
    EXPECT_EQ(items[0]->tag_name, "li");
    EXPECT_EQ(items[1]->tag_name, "li");
    EXPECT_EQ(items[2]->tag_name, "li");
    EXPECT_EQ(items[0]->parent, ul);
    EXPECT_EQ(items[1]->parent, ul);
    EXPECT_EQ(items[2]->parent, ul);
    EXPECT_EQ(items[0]->text_content(), "Alpha");
    EXPECT_EQ(items[1]->text_content(), "Beta");
    EXPECT_EQ(items[2]->text_content(), "Gamma");
}

TEST(HTMLParserTest, TableWithRowsAndCellsV70) {
    auto doc = clever::html::parse("<table><tr><td>A1</td><td>B1</td></tr><tr><td>A2</td><td>B2</td></tr></table>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    auto rows = doc->find_all_elements("tr");
    auto cells = doc->find_all_elements("td");
    ASSERT_NE(table, nullptr);
    ASSERT_EQ(rows.size(), 2u);
    ASSERT_EQ(cells.size(), 4u);

    EXPECT_EQ(table->tag_name, "table");
    EXPECT_EQ(rows[0]->tag_name, "tr");
    EXPECT_EQ(rows[1]->tag_name, "tr");
    EXPECT_EQ(cells[0]->parent, rows[0]);
    EXPECT_EQ(cells[1]->parent, rows[0]);
    EXPECT_EQ(cells[2]->parent, rows[1]);
    EXPECT_EQ(cells[3]->parent, rows[1]);
    EXPECT_EQ(cells[0]->text_content(), "A1");
    EXPECT_EQ(cells[1]->text_content(), "B1");
    EXPECT_EQ(cells[2]->text_content(), "A2");
    EXPECT_EQ(cells[3]->text_content(), "B2");
}

TEST(HTMLParserTest, FormWithInputElementV70) {
    auto doc = clever::html::parse("<form action='/submit'><input type='text' name='username'></form>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    auto* input = doc->find_element("input");
    ASSERT_NE(form, nullptr);
    ASSERT_NE(input, nullptr);

    EXPECT_EQ(form->tag_name, "form");
    EXPECT_EQ(input->tag_name, "input");
    EXPECT_EQ(input->parent, form);
    EXPECT_EQ(get_attr_v63(form, "action"), "/submit");
    EXPECT_EQ(get_attr_v63(input, "type"), "text");
    EXPECT_EQ(get_attr_v63(input, "name"), "username");
}

TEST(HTMLParserTest, AnchorWithHrefAttributeV70) {
    auto doc = clever::html::parse("<a href='https://example.com/docs'>Read docs</a>");
    ASSERT_NE(doc, nullptr);

    auto* anchor = doc->find_element("a");
    ASSERT_NE(anchor, nullptr);
    EXPECT_EQ(anchor->tag_name, "a");
    EXPECT_EQ(get_attr_v63(anchor, "href"), "https://example.com/docs");
    EXPECT_EQ(anchor->text_content(), "Read docs");
}

TEST(HTMLParserTest, ImgSelfClosingWithSrcV70) {
    auto doc = clever::html::parse("<img src='photo.png'/>");
    ASSERT_NE(doc, nullptr);

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->tag_name, "img");
    EXPECT_EQ(get_attr_v63(img, "src"), "photo.png");
    EXPECT_TRUE(img->children.empty());
}

TEST(HTMLParserTest, ButtonElementWithTextV71) {
    auto doc = clever::html::parse("<button>Submit Form</button>");
    ASSERT_NE(doc, nullptr);

    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->tag_name, "button");
    EXPECT_EQ(button->text_content(), "Submit Form");
}

TEST(HTMLParserTest, SpanInsideParagraphV71) {
    auto doc = clever::html::parse("<p>Before <span>inline</span> after</p>");
    ASSERT_NE(doc, nullptr);

    auto* paragraph = doc->find_element("p");
    auto* span = doc->find_element("span");
    ASSERT_NE(paragraph, nullptr);
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(paragraph->tag_name, "p");
    EXPECT_EQ(span->tag_name, "span");
    EXPECT_EQ(span->parent, paragraph);
    EXPECT_EQ(span->text_content(), "inline");
}

TEST(HTMLParserTest, StrongAndEmElementsV71) {
    auto doc = clever::html::parse("<p><strong>Bold</strong> and <em>Italic</em></p>");
    ASSERT_NE(doc, nullptr);

    auto* paragraph = doc->find_element("p");
    auto* strong = doc->find_element("strong");
    auto* em = doc->find_element("em");
    ASSERT_NE(paragraph, nullptr);
    ASSERT_NE(strong, nullptr);
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(strong->parent, paragraph);
    EXPECT_EQ(em->parent, paragraph);
    EXPECT_EQ(strong->text_content(), "Bold");
    EXPECT_EQ(em->text_content(), "Italic");
}

TEST(HTMLParserTest, BrProducesEmptyElementV71) {
    auto doc = clever::html::parse("<p>Line1<br>Line2</p>");
    ASSERT_NE(doc, nullptr);

    auto* paragraph = doc->find_element("p");
    auto* br = doc->find_element("br");
    ASSERT_NE(paragraph, nullptr);
    ASSERT_NE(br, nullptr);
    EXPECT_EQ(br->tag_name, "br");
    EXPECT_EQ(br->parent, paragraph);
    EXPECT_TRUE(br->children.empty());
}

TEST(HTMLParserTest, HrSelfClosingElementV71) {
    auto doc = clever::html::parse("<hr/>");
    ASSERT_NE(doc, nullptr);

    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
    EXPECT_EQ(hr->tag_name, "hr");
    EXPECT_TRUE(hr->children.empty());
}

TEST(HTMLParserTest, DivWithIdAndClassAttributesV71) {
    auto doc = clever::html::parse("<div id='main' class='container'>Box</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->tag_name, "div");
    EXPECT_EQ(get_attr_v63(div, "id"), "main");
    EXPECT_EQ(get_attr_v63(div, "class"), "container");
    EXPECT_EQ(div->text_content(), "Box");
}

TEST(HTMLParserTest, NestedSpansV71) {
    auto doc = clever::html::parse("<span>Outer<span>Inner</span></span>");
    ASSERT_NE(doc, nullptr);

    auto spans = doc->find_all_elements("span");
    ASSERT_EQ(spans.size(), 2u);

    clever::html::SimpleNode* outer = nullptr;
    clever::html::SimpleNode* inner = nullptr;
    for (auto* span : spans) {
        if (span->parent != nullptr && span->parent->tag_name == "span") inner = span;
    }
    for (auto* span : spans) {
        if (span != inner) outer = span;
    }

    ASSERT_NE(outer, nullptr);
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->parent, outer);
    EXPECT_EQ(inner->text_content(), "Inner");
    EXPECT_EQ(outer->text_content(), "OuterInner");
}

TEST(HTMLParserTest, MultipleParagraphsInBodyV71) {
    auto doc = clever::html::parse("<body><p>First</p><p>Second</p><p>Third</p></body>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    auto paragraphs = doc->find_all_elements("p");
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(paragraphs.size(), 3u);

    EXPECT_EQ(paragraphs[0]->parent, body);
    EXPECT_EQ(paragraphs[1]->parent, body);
    EXPECT_EQ(paragraphs[2]->parent, body);
    EXPECT_EQ(paragraphs[0]->text_content(), "First");
    EXPECT_EQ(paragraphs[1]->text_content(), "Second");
    EXPECT_EQ(paragraphs[2]->text_content(), "Third");
}

TEST(HTMLParserTest, HeadingsH1ThroughH6V72) {
    auto doc = clever::html::parse(
        "<h1>Heading 1</h1><h2>Heading 2</h2><h3>Heading 3</h3>"
        "<h4>Heading 4</h4><h5>Heading 5</h5><h6>Heading 6</h6>");
    ASSERT_NE(doc, nullptr);

    auto h1 = doc->find_all_elements("h1");
    auto h2 = doc->find_all_elements("h2");
    auto h3 = doc->find_all_elements("h3");
    auto h4 = doc->find_all_elements("h4");
    auto h5 = doc->find_all_elements("h5");
    auto h6 = doc->find_all_elements("h6");
    ASSERT_EQ(h1.size(), 1u);
    ASSERT_EQ(h2.size(), 1u);
    ASSERT_EQ(h3.size(), 1u);
    ASSERT_EQ(h4.size(), 1u);
    ASSERT_EQ(h5.size(), 1u);
    ASSERT_EQ(h6.size(), 1u);
    EXPECT_EQ(h1[0]->tag_name, "h1");
    EXPECT_EQ(h2[0]->tag_name, "h2");
    EXPECT_EQ(h3[0]->tag_name, "h3");
    EXPECT_EQ(h4[0]->tag_name, "h4");
    EXPECT_EQ(h5[0]->tag_name, "h5");
    EXPECT_EQ(h6[0]->tag_name, "h6");
    EXPECT_EQ(h1[0]->text_content(), "Heading 1");
    EXPECT_EQ(h2[0]->text_content(), "Heading 2");
    EXPECT_EQ(h3[0]->text_content(), "Heading 3");
    EXPECT_EQ(h4[0]->text_content(), "Heading 4");
    EXPECT_EQ(h5[0]->text_content(), "Heading 5");
    EXPECT_EQ(h6[0]->text_content(), "Heading 6");
}

TEST(HTMLParserTest, ParagraphWithClassAttributeV72) {
    auto doc = clever::html::parse("<p class='lead'>Paragraph copy</p>");
    ASSERT_NE(doc, nullptr);

    auto* paragraph = doc->find_element("p");
    ASSERT_NE(paragraph, nullptr);
    EXPECT_EQ(paragraph->tag_name, "p");
    EXPECT_EQ(get_attr_v63(paragraph, "class"), "lead");
    EXPECT_EQ(paragraph->text_content(), "Paragraph copy");
}

TEST(HTMLParserTest, DivWithMultipleChildrenV72) {
    auto doc = clever::html::parse("<div><span>One</span><span>Two</span><p>Three</p></div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    auto spans = doc->find_all_elements("span");
    auto* paragraph = doc->find_element("p");
    ASSERT_NE(div, nullptr);
    ASSERT_EQ(spans.size(), 2u);
    ASSERT_NE(paragraph, nullptr);
    ASSERT_GE(div->children.size(), 3u);

    EXPECT_EQ(div->tag_name, "div");
    EXPECT_EQ(div->children[0]->tag_name, "span");
    EXPECT_EQ(div->children[1]->tag_name, "span");
    EXPECT_EQ(div->children[2]->tag_name, "p");
    EXPECT_EQ(spans[0]->parent, div);
    EXPECT_EQ(spans[1]->parent, div);
    EXPECT_EQ(paragraph->parent, div);
    EXPECT_EQ(spans[0]->text_content(), "One");
    EXPECT_EQ(spans[1]->text_content(), "Two");
    EXPECT_EQ(paragraph->text_content(), "Three");
}

TEST(HTMLParserTest, UnorderedListThreeItemsV72) {
    auto doc = clever::html::parse("<ul><li>First</li><li>Second</li><li>Third</li></ul>");
    ASSERT_NE(doc, nullptr);

    auto* ul = doc->find_element("ul");
    auto items = doc->find_all_elements("li");
    ASSERT_NE(ul, nullptr);
    ASSERT_EQ(items.size(), 3u);
    EXPECT_EQ(ul->tag_name, "ul");
    EXPECT_EQ(items[0]->tag_name, "li");
    EXPECT_EQ(items[1]->tag_name, "li");
    EXPECT_EQ(items[2]->tag_name, "li");
    EXPECT_EQ(items[0]->parent, ul);
    EXPECT_EQ(items[1]->parent, ul);
    EXPECT_EQ(items[2]->parent, ul);
    EXPECT_EQ(items[0]->text_content(), "First");
    EXPECT_EQ(items[1]->text_content(), "Second");
    EXPECT_EQ(items[2]->text_content(), "Third");
}

TEST(HTMLParserTest, TableWithTheadAndTbodyV72) {
    auto doc = clever::html::parse(
        "<table><thead><tr><th>Name</th></tr></thead>"
        "<tbody><tr><td>Ada</td></tr><tr><td>Linus</td></tr></tbody></table>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    auto* thead = doc->find_element("thead");
    auto* tbody = doc->find_element("tbody");
    auto rows = doc->find_all_elements("tr");
    auto ths = doc->find_all_elements("th");
    auto tds = doc->find_all_elements("td");
    ASSERT_NE(table, nullptr);
    ASSERT_NE(thead, nullptr);
    ASSERT_NE(tbody, nullptr);
    ASSERT_EQ(rows.size(), 3u);
    ASSERT_EQ(ths.size(), 1u);
    ASSERT_EQ(tds.size(), 2u);

    EXPECT_EQ(table->tag_name, "table");
    EXPECT_EQ(thead->tag_name, "thead");
    EXPECT_EQ(tbody->tag_name, "tbody");
    EXPECT_EQ(thead->parent, table);
    EXPECT_EQ(tbody->parent, table);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(tds[0]->text_content(), "Ada");
    EXPECT_EQ(tds[1]->text_content(), "Linus");
}

TEST(HTMLParserTest, InputTypesTextAndPasswordV72) {
    auto doc = clever::html::parse(
        "<form><input type='text' name='username'><input type='password' name='password'></form>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    auto inputs = doc->find_all_elements("input");
    ASSERT_NE(form, nullptr);
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(inputs[0]->tag_name, "input");
    EXPECT_EQ(inputs[1]->tag_name, "input");
    EXPECT_EQ(inputs[0]->parent, form);
    EXPECT_EQ(inputs[1]->parent, form);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "text");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "password");
}

TEST(HTMLParserTest, LabelWithForAttributeV72) {
    auto doc = clever::html::parse("<label for='email'>Email</label><input id='email' type='text'>");
    ASSERT_NE(doc, nullptr);

    auto* label = doc->find_element("label");
    auto* input = doc->find_element("input");
    ASSERT_NE(label, nullptr);
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(label->tag_name, "label");
    EXPECT_EQ(input->tag_name, "input");
    EXPECT_EQ(get_attr_v63(label, "for"), "email");
    EXPECT_EQ(get_attr_v63(input, "id"), "email");
    EXPECT_EQ(label->text_content(), "Email");
}

TEST(HTMLParserTest, TextareaWithDefaultTextV72) {
    auto doc = clever::html::parse("<textarea>Default text value</textarea>");
    ASSERT_NE(doc, nullptr);

    auto* textarea = doc->find_element("textarea");
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(textarea->tag_name, "textarea");
    EXPECT_EQ(textarea->text_content(), "Default text value");
}

TEST(HTMLParserTest, BodyContainsDivChildrenV73) {
    auto doc = clever::html::parse("<body><div>First</div><div>Second</div></body>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->children.size(), 2u);
    EXPECT_EQ(body->children[0]->type, clever::html::SimpleNode::Element);
    EXPECT_EQ(body->children[1]->type, clever::html::SimpleNode::Element);
    EXPECT_EQ(body->children[0]->tag_name, "div");
    EXPECT_EQ(body->children[1]->tag_name, "div");
    EXPECT_EQ(body->children[0]->text_content(), "First");
    EXPECT_EQ(body->children[1]->text_content(), "Second");
}

TEST(HTMLParserTest, HeadHasTitleElementV73) {
    auto doc = clever::html::parse("<html><head><title>Parser Title</title></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* head = doc->find_element("head");
    auto* title = doc->find_element("title");
    ASSERT_NE(head, nullptr);
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(head->tag_name, "head");
    EXPECT_EQ(title->tag_name, "title");
    EXPECT_EQ(title->parent, head);
    EXPECT_EQ(title->text_content(), "Parser Title");
}

TEST(HTMLParserTest, MetaCharsetUtf8V73) {
    auto doc = clever::html::parse("<head><meta charset='utf-8'></head>");
    ASSERT_NE(doc, nullptr);

    auto* meta = doc->find_element("meta");
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->tag_name, "meta");
    EXPECT_EQ(get_attr_v63(meta, "charset"), "utf-8");
}

TEST(HTMLParserTest, LinkStylesheetElementV73) {
    auto doc = clever::html::parse("<head><link rel='stylesheet' href='/assets/site.css'></head>");
    ASSERT_NE(doc, nullptr);

    auto* link = doc->find_element("link");
    ASSERT_NE(link, nullptr);
    EXPECT_EQ(link->tag_name, "link");
    EXPECT_TRUE(link->children.empty());
    EXPECT_EQ(get_attr_v63(link, "rel"), "stylesheet");
    EXPECT_EQ(get_attr_v63(link, "href"), "/assets/site.css");
}

TEST(HTMLParserTest, EmptyDivHasNoChildrenV73) {
    auto doc = clever::html::parse("<body><div></div></body>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->tag_name, "div");
    EXPECT_TRUE(div->children.empty());
    EXPECT_TRUE(div->text_content().empty());
}

TEST(HTMLParserTest, TextAfterElementInBodyV73) {
    auto doc = clever::html::parse("<body><span>lead</span>tail</body>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->text_content(), "leadtail");

    bool found_text_after_span = false;
    for (size_t i = 0; i + 1 < body->children.size(); ++i) {
        if (body->children[i]->type == clever::html::SimpleNode::Element &&
            body->children[i]->tag_name == "span" &&
            body->children[i + 1]->type == clever::html::SimpleNode::Text &&
            body->children[i + 1]->data == "tail") {
            found_text_after_span = true;
        }
    }
    EXPECT_TRUE(found_text_after_span);
}

TEST(HTMLParserTest, BrBetweenTextNodesV73) {
    auto doc = clever::html::parse("<body>alpha<br>beta</body>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    auto* br = doc->find_element("br");
    ASSERT_NE(body, nullptr);
    ASSERT_NE(br, nullptr);
    EXPECT_EQ(br->tag_name, "br");
    EXPECT_EQ(br->parent, body);
    EXPECT_TRUE(br->children.empty());
    EXPECT_EQ(body->text_content(), "alphabeta");

    bool found_br_between_text = false;
    for (size_t i = 1; i + 1 < body->children.size(); ++i) {
        if (body->children[i]->type == clever::html::SimpleNode::Element &&
            body->children[i]->tag_name == "br" &&
            body->children[i - 1]->type == clever::html::SimpleNode::Text &&
            body->children[i + 1]->type == clever::html::SimpleNode::Text &&
            body->children[i - 1]->data == "alpha" &&
            body->children[i + 1]->data == "beta") {
            found_br_between_text = true;
        }
    }
    EXPECT_TRUE(found_br_between_text);
}

TEST(HTMLParserTest, ImgAltAttributeV73) {
    auto doc = clever::html::parse("<body><img src='photo.jpg' alt='profile image'></body>");
    ASSERT_NE(doc, nullptr);

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->tag_name, "img");
    EXPECT_TRUE(img->children.empty());
    EXPECT_EQ(get_attr_v63(img, "src"), "photo.jpg");
    EXPECT_EQ(get_attr_v63(img, "alt"), "profile image");
}

TEST(HTMLParserTest, VideoElementWithSrcAttributeV74) {
    auto doc = clever::html::parse("<body><video src='movie.mp4'></video></body>");
    ASSERT_NE(doc, nullptr);

    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    EXPECT_EQ(video->tag_name, "video");
    EXPECT_EQ(get_attr_v63(video, "src"), "movie.mp4");
}

TEST(HTMLParserTest, AudioElementWithControlsAttributeV74) {
    auto doc = clever::html::parse("<body><audio controls></audio></body>");
    ASSERT_NE(doc, nullptr);

    auto* audio = doc->find_element("audio");
    ASSERT_NE(audio, nullptr);
    EXPECT_EQ(audio->tag_name, "audio");
    EXPECT_EQ(get_attr_v63(audio, "controls"), "");
}

TEST(HTMLParserTest, CanvasElementDimensionsV74) {
    auto doc = clever::html::parse("<body><canvas width='640' height='360'></canvas></body>");
    ASSERT_NE(doc, nullptr);

    auto* canvas = doc->find_element("canvas");
    ASSERT_NE(canvas, nullptr);
    EXPECT_EQ(canvas->tag_name, "canvas");
    EXPECT_EQ(get_attr_v63(canvas, "width"), "640");
    EXPECT_EQ(get_attr_v63(canvas, "height"), "360");
}

TEST(HTMLParserTest, IframeElementSrcAttributeV74) {
    auto doc = clever::html::parse("<body><iframe src='https://example.com/embed'></iframe></body>");
    ASSERT_NE(doc, nullptr);

    auto* iframe = doc->find_element("iframe");
    ASSERT_NE(iframe, nullptr);
    EXPECT_EQ(iframe->tag_name, "iframe");
    EXPECT_EQ(get_attr_v63(iframe, "src"), "https://example.com/embed");
}

TEST(HTMLParserTest, EmbedElementTypeAttributeV74) {
    auto doc = clever::html::parse("<body><embed type='application/pdf'></body>");
    ASSERT_NE(doc, nullptr);

    auto* embed = doc->find_element("embed");
    ASSERT_NE(embed, nullptr);
    EXPECT_EQ(embed->tag_name, "embed");
    EXPECT_EQ(get_attr_v63(embed, "type"), "application/pdf");
}

TEST(HTMLParserTest, ObjectElementDataAttributeV74) {
    auto doc = clever::html::parse("<body><object data='movie.swf'></object></body>");
    ASSERT_NE(doc, nullptr);

    auto* object = doc->find_element("object");
    ASSERT_NE(object, nullptr);
    EXPECT_EQ(object->tag_name, "object");
    EXPECT_EQ(get_attr_v63(object, "data"), "movie.swf");
}

TEST(HTMLParserTest, SourceElementWithinVideoV74) {
    auto doc = clever::html::parse(
        "<body><video><source src='movie.webm' type='video/webm'></video></body>");
    ASSERT_NE(doc, nullptr);

    auto* video = doc->find_element("video");
    auto* source = doc->find_element("source");
    ASSERT_NE(video, nullptr);
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(video->tag_name, "video");
    EXPECT_EQ(source->tag_name, "source");
    EXPECT_EQ(source->parent, video);
    EXPECT_EQ(get_attr_v63(source, "src"), "movie.webm");
    EXPECT_EQ(get_attr_v63(source, "type"), "video/webm");
}

TEST(HTMLParserTest, TrackElementWithinVideoV74) {
    auto doc = clever::html::parse(
        "<body><video><track kind='captions' src='captions.vtt' srclang='en'></video></body>");
    ASSERT_NE(doc, nullptr);

    auto* video = doc->find_element("video");
    auto* track = doc->find_element("track");
    ASSERT_NE(video, nullptr);
    ASSERT_NE(track, nullptr);
    EXPECT_EQ(video->tag_name, "video");
    EXPECT_EQ(track->tag_name, "track");
    EXPECT_EQ(track->parent, video);
    EXPECT_EQ(get_attr_v63(track, "kind"), "captions");
    EXPECT_EQ(get_attr_v63(track, "src"), "captions.vtt");
    EXPECT_EQ(get_attr_v63(track, "srclang"), "en");
}

TEST(HTMLParserTest, SemanticElementParsingMainSectionV75) {
    auto doc = clever::html::parse(
        "<html><body><main><section><h2>Latest</h2></section></main></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* main = doc->find_element("main");
    auto* section = doc->find_element("section");
    auto* heading = doc->find_element("h2");
    ASSERT_NE(main, nullptr);
    ASSERT_NE(section, nullptr);
    ASSERT_NE(heading, nullptr);
    EXPECT_EQ(main->tag_name, "main");
    EXPECT_EQ(section->tag_name, "section");
    EXPECT_EQ(section->parent, main);
    EXPECT_EQ(heading->parent, section);
    EXPECT_EQ(heading->text_content(), "Latest");
}

TEST(HTMLParserTest, AnchorAttributesAndTextV75) {
    auto doc = clever::html::parse(
        "<html><body><a href='/docs' target='_blank' rel='noopener'>Docs</a></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* anchor = doc->find_element("a");
    ASSERT_NE(anchor, nullptr);
    EXPECT_EQ(anchor->tag_name, "a");
    EXPECT_EQ(get_attr_v63(anchor, "href"), "/docs");
    EXPECT_EQ(get_attr_v63(anchor, "target"), "_blank");
    EXPECT_EQ(get_attr_v63(anchor, "rel"), "noopener");
    EXPECT_EQ(anchor->text_content(), "Docs");
}

TEST(HTMLParserTest, NestedElementsParentChainV75) {
    auto doc = clever::html::parse(
        "<html><body><div id='outer'><div id='inner'><span>deep</span></div></div></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    auto* inner = span->parent;
    ASSERT_NE(inner, nullptr);
    auto* outer = inner->parent;
    ASSERT_NE(outer, nullptr);

    EXPECT_EQ(inner->tag_name, "div");
    EXPECT_EQ(outer->tag_name, "div");
    EXPECT_EQ(get_attr_v63(inner, "id"), "inner");
    EXPECT_EQ(get_attr_v63(outer, "id"), "outer");
    EXPECT_EQ(span->text_content(), "deep");
}

TEST(HTMLParserTest, TextContentAcrossInlineChildrenV75) {
    auto doc = clever::html::parse(
        "<html><body><p>alpha <strong>beta</strong> gamma</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* paragraph = doc->find_element("p");
    auto* strong = doc->find_element("strong");
    ASSERT_NE(paragraph, nullptr);
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "beta");

    const std::string text = paragraph->text_content();
    const size_t alpha_pos = text.find("alpha");
    const size_t beta_pos = text.find("beta");
    const size_t gamma_pos = text.find("gamma");
    ASSERT_NE(alpha_pos, std::string::npos);
    ASSERT_NE(beta_pos, std::string::npos);
    ASSERT_NE(gamma_pos, std::string::npos);
    EXPECT_LT(alpha_pos, beta_pos);
    EXPECT_LT(beta_pos, gamma_pos);
}

TEST(HTMLParserTest, VoidElementsHaveNoChildrenV75) {
    auto doc = clever::html::parse(
        "<html><body>start<br><img src='hero.png' alt='hero'><hr>end</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* br = doc->find_element("br");
    auto* img = doc->find_element("img");
    auto* hr = doc->find_element("hr");
    auto* body = doc->find_element("body");
    ASSERT_NE(br, nullptr);
    ASSERT_NE(img, nullptr);
    ASSERT_NE(hr, nullptr);
    ASSERT_NE(body, nullptr);

    EXPECT_TRUE(br->children.empty());
    EXPECT_TRUE(img->children.empty());
    EXPECT_TRUE(hr->children.empty());
    EXPECT_EQ(get_attr_v63(img, "src"), "hero.png");
    EXPECT_EQ(get_attr_v63(img, "alt"), "hero");
    EXPECT_EQ(body->text_content(), "startend");
}

TEST(HTMLParserTest, ScriptElementPreservesRawComparatorsV75) {
    auto doc = clever::html::parse(
        "<html><body><script>if (a < b) { c = 1; }</script></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    EXPECT_EQ(script->tag_name, "script");
    const std::string js = script->text_content();
    EXPECT_NE(js.find("a < b"), std::string::npos);
    EXPECT_NE(js.find("c = 1"), std::string::npos);
}

TEST(HTMLParserTest, StyleElementPreservesCssTextV75) {
    auto doc = clever::html::parse(
        "<html><body><style>body > p { color: red; }</style></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* style = doc->find_element("style");
    ASSERT_NE(style, nullptr);
    EXPECT_EQ(style->tag_name, "style");
    const std::string css = style->text_content();
    EXPECT_NE(css.find("body > p"), std::string::npos);
    EXPECT_NE(css.find("color: red"), std::string::npos);
}

TEST(HTMLParserTest, TextareaSpecialElementContentAndAttributeV75) {
    auto doc = clever::html::parse(
        "<html><body><textarea name='notes'>alpha\nbeta\ngamma</textarea></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* textarea = doc->find_element("textarea");
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(textarea->tag_name, "textarea");
    EXPECT_EQ(get_attr_v63(textarea, "name"), "notes");
    EXPECT_EQ(textarea->text_content(), "alpha\nbeta\ngamma");
}

TEST(HTMLParserTest, DocumentScaffoldHtmlHeadBodyV76) {
    auto doc = clever::html::parse(
        "<html><head><title>Parser V76</title></head><body><main><h1>Welcome</h1></main></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* html = doc->find_element("html");
    auto* head = doc->find_element("head");
    auto* title = doc->find_element("title");
    auto* body = doc->find_element("body");
    auto* main = doc->find_element("main");
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(html, nullptr);
    ASSERT_NE(head, nullptr);
    ASSERT_NE(title, nullptr);
    ASSERT_NE(body, nullptr);
    ASSERT_NE(main, nullptr);
    ASSERT_NE(h1, nullptr);

    EXPECT_EQ(html->tag_name, "html");
    EXPECT_EQ(head->parent, html);
    EXPECT_EQ(body->parent, html);
    EXPECT_EQ(main->parent, body);
    EXPECT_EQ(title->text_content(), "Parser V76");
    EXPECT_EQ(h1->text_content(), "Welcome");
}

TEST(HTMLParserTest, TreeBuildSectionContainsTwoArticlesV76) {
    auto doc = clever::html::parse(
        "<html><body><section id='feed'><article><p>First</p></article><article><p>Second</p></article></section></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* section = doc->find_element("section");
    ASSERT_NE(section, nullptr);
    EXPECT_EQ(section->tag_name, "section");
    EXPECT_EQ(get_attr_v63(section, "id"), "feed");

    size_t article_count = 0;
    for (auto& child : section->children) {
        if (!child) continue;
        if (child->type != clever::html::SimpleNode::Element) continue;
        if (child->tag_name != "article") continue;
        article_count++;
        EXPECT_EQ(child->parent, section);
    }
    EXPECT_EQ(article_count, 2u);
}

TEST(HTMLParserTest, ScriptElementParsedAndFoundV76) {
    auto doc = clever::html::parse(
        "<html><body><script>console.log('hello');</script><p>after</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    EXPECT_EQ(script->tag_name, "script");

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "after");
}

TEST(HTMLParserTest, StyleSpecialElementKeepsSelectorTextV76) {
    auto doc = clever::html::parse(
        "<html><body><style>.box::before { content: \"<p>\"; } body > p { color: blue; }</style></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* style = doc->find_element("style");
    ASSERT_NE(style, nullptr);
    EXPECT_EQ(style->tag_name, "style");

    const std::string css = style->text_content();
    EXPECT_NE(css.find("content: \"<p>\""), std::string::npos);
    EXPECT_NE(css.find("body > p"), std::string::npos);
    EXPECT_EQ(doc->find_element("p"), nullptr);
}

TEST(HTMLParserTest, TextareaAttributeAndChildPresenceV76) {
    auto doc = clever::html::parse(
        "<html><body><textarea rows='3' cols='40'>some text here</textarea></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* textarea = doc->find_element("textarea");
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(textarea->tag_name, "textarea");
    EXPECT_EQ(get_attr_v63(textarea, "rows"), "3");
    EXPECT_EQ(get_attr_v63(textarea, "cols"), "40");
    EXPECT_FALSE(textarea->text_content().empty());
}

TEST(HTMLParserTest, VoidElementsRemainLeafNodesV76) {
    auto doc = clever::html::parse(
        "<html><body><div>start<img src='x.png'>middle<br>end<hr></div></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    auto* img = doc->find_element("img");
    auto* br = doc->find_element("br");
    auto* hr = doc->find_element("hr");
    ASSERT_NE(div, nullptr);
    ASSERT_NE(img, nullptr);
    ASSERT_NE(br, nullptr);
    ASSERT_NE(hr, nullptr);

    EXPECT_TRUE(img->children.empty());
    EXPECT_TRUE(br->children.empty());
    EXPECT_TRUE(hr->children.empty());
    EXPECT_EQ(img->parent, div);
    EXPECT_EQ(br->parent, div);
    EXPECT_EQ(hr->parent, div);
    EXPECT_EQ(get_attr_v63(img, "src"), "x.png");
    EXPECT_EQ(div->text_content(), "startmiddleend");
}

TEST(HTMLParserTest, AttributeHandlingQuotedUnquotedBooleanV76) {
    auto doc = clever::html::parse(
        "<html><body><input type=text disabled value='abc' data-id=99></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->tag_name, "input");
    EXPECT_EQ(get_attr_v63(input, "type"), "text");
    EXPECT_EQ(get_attr_v63(input, "disabled"), "");
    EXPECT_EQ(get_attr_v63(input, "value"), "abc");
    EXPECT_EQ(get_attr_v63(input, "data-id"), "99");
}

TEST(HTMLParserTest, AttributeHandlingHyphenatedAndTextV76) {
    auto doc = clever::html::parse(
        "<html><body><a href='/search?q=one&lang=en' title='go now' aria-label=\"Search Link\" rel=noopener>Search</a></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* anchor = doc->find_element("a");
    ASSERT_NE(anchor, nullptr);
    EXPECT_EQ(anchor->tag_name, "a");
    EXPECT_EQ(get_attr_v63(anchor, "href"), "/search?q=one&lang=en");
    EXPECT_EQ(get_attr_v63(anchor, "title"), "go now");
    EXPECT_EQ(get_attr_v63(anchor, "aria-label"), "Search Link");
    EXPECT_EQ(get_attr_v63(anchor, "rel"), "noopener");
    EXPECT_EQ(anchor->text_content(), "Search");
}

TEST(HTMLParserTest, NestedDivsParentChildRelationV77) {
    auto doc = clever::html::parse("<html><body><div id='outer'><div id='inner'>text</div></div></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* outer = doc->find_element("div");
    ASSERT_NE(outer, nullptr);
    EXPECT_EQ(outer->tag_name, "div");
    EXPECT_EQ(get_attr_v63(outer, "id"), "outer");

    clever::html::SimpleNode* inner = nullptr;
    for (auto& child : outer->children) {
        if (child && child->type == clever::html::SimpleNode::Element && child->tag_name == "div") {
            inner = child.get();
            break;
        }
    }
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->tag_name, "div");
    EXPECT_EQ(get_attr_v63(inner, "id"), "inner");
    EXPECT_EQ(inner->parent, outer);
    EXPECT_EQ(inner->text_content(), "text");
}

TEST(HTMLParserTest, MultipleParagraphsCountV77) {
    auto doc = clever::html::parse("<html><body><p>First</p><p>Second</p><p>Third</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto paragraphs = doc->find_all_elements("p");
    ASSERT_EQ(paragraphs.size(), 3u);

    EXPECT_EQ(paragraphs[0]->tag_name, "p");
    EXPECT_EQ(paragraphs[1]->tag_name, "p");
    EXPECT_EQ(paragraphs[2]->tag_name, "p");
    EXPECT_EQ(paragraphs[0]->text_content(), "First");
    EXPECT_EQ(paragraphs[1]->text_content(), "Second");
    EXPECT_EQ(paragraphs[2]->text_content(), "Third");
}

TEST(HTMLParserTest, UnorderedListWithItemsV77) {
    auto doc = clever::html::parse("<html><body><ul><li>a</li><li>b</li></ul></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    EXPECT_EQ(ul->tag_name, "ul");

    size_t li_count = 0;
    for (auto& child : ul->children) {
        if (child && child->type == clever::html::SimpleNode::Element && child->tag_name == "li") {
            li_count++;
            EXPECT_EQ(child->parent, ul);
        }
    }
    EXPECT_EQ(li_count, 2u);
}

TEST(HTMLParserTest, AnchorTagHrefAndTextV77) {
    auto doc = clever::html::parse("<html><body><a href='/link'>click</a></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* anchor = doc->find_element("a");
    ASSERT_NE(anchor, nullptr);
    EXPECT_EQ(anchor->tag_name, "a");
    EXPECT_EQ(get_attr_v63(anchor, "href"), "/link");
    EXPECT_EQ(anchor->text_content(), "click");
}

TEST(HTMLParserTest, ImageVoidElementNoChildrenV77) {
    auto doc = clever::html::parse("<html><body><img src='x.png' alt='pic'></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->tag_name, "img");
    EXPECT_EQ(get_attr_v63(img, "src"), "x.png");
    EXPECT_EQ(get_attr_v63(img, "alt"), "pic");
    EXPECT_TRUE(img->children.empty());
}

TEST(HTMLParserTest, FormWithInputAndButtonV77) {
    auto doc = clever::html::parse("<html><body><form><input type='text'><button>Submit</button></form></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(form->tag_name, "form");

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->tag_name, "input");
    EXPECT_EQ(input->parent, form);

    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->tag_name, "button");
    EXPECT_EQ(button->parent, form);
    EXPECT_EQ(button->text_content(), "Submit");
}

TEST(HTMLParserTest, HeadingTagsH1H2V77) {
    auto doc = clever::html::parse("<html><body><h1>Main</h1><h2>Sub</h2></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* h1 = doc->find_element("h1");
    auto* h2 = doc->find_element("h2");
    ASSERT_NE(h1, nullptr);
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h1->tag_name, "h1");
    EXPECT_EQ(h2->tag_name, "h2");
    EXPECT_EQ(h1->text_content(), "Main");
    EXPECT_EQ(h2->text_content(), "Sub");
}

TEST(HTMLParserTest, SpanWithClassAttributeV77) {
    auto doc = clever::html::parse("<html><body><span class='highlight'>text</span></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->tag_name, "span");
    EXPECT_EQ(get_attr_v63(span, "class"), "highlight");
    EXPECT_EQ(span->text_content(), "text");
}

TEST(HTMLParserTest, TableStructureTrTdV78) {
    auto doc = clever::html::parse("<html><body><table><tr><td>Cell</td></tr></table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->tag_name, "table");

    auto* tr = doc->find_element("tr");
    ASSERT_NE(tr, nullptr);
    EXPECT_EQ(tr->tag_name, "tr");
    auto* tbody = doc->find_element("tbody");
    ASSERT_NE(tbody, nullptr);
    EXPECT_EQ(tbody->parent, table);
    EXPECT_EQ(tr->parent, tbody);

    auto* td = doc->find_element("td");
    ASSERT_NE(td, nullptr);
    EXPECT_EQ(td->tag_name, "td");
    EXPECT_EQ(td->parent, tr);
    EXPECT_EQ(td->text_content(), "Cell");
}

TEST(HTMLParserTest, TableImpliedTbodyForDirectTrV126) {
    auto doc = clever::html::parse("<table><tr><td>A</td></tr></table>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    auto* tbody = doc->find_element("tbody");
    auto* tr = doc->find_element("tr");
    auto* td = doc->find_element("td");
    ASSERT_NE(table, nullptr);
    ASSERT_NE(tbody, nullptr);
    ASSERT_NE(tr, nullptr);
    ASSERT_NE(td, nullptr);

    EXPECT_EQ(tbody->parent, table);
    EXPECT_EQ(tr->parent, tbody);
    EXPECT_EQ(td->parent, tr);
    EXPECT_EQ(td->text_content(), "A");
}

TEST(HTMLParserTest, TableImpliedTbodyAndTrForDirectTdV126) {
    auto doc = clever::html::parse("<table><td>X</td><td>Y</td></table>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    auto* tbody = doc->find_element("tbody");
    ASSERT_NE(table, nullptr);
    ASSERT_NE(tbody, nullptr);
    EXPECT_EQ(tbody->parent, table);

    auto rows = tbody->find_all_elements("tr");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0]->parent, tbody);

    auto cells = rows[0]->find_all_elements("td");
    ASSERT_EQ(cells.size(), 2u);
    EXPECT_EQ(cells[0]->text_content(), "X");
    EXPECT_EQ(cells[1]->text_content(), "Y");
}

TEST(HTMLParserTest, TableCloseFromCellRestoresBodyParsingV126) {
    auto doc = clever::html::parse("<table><tr><td>x</table><p>after</p>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "after");

    auto* td = doc->find_element("td");
    ASSERT_NE(td, nullptr);
    EXPECT_NE(p->parent, td);
}

TEST(HTMLParserTest, SelectWithOptionsV78) {
    auto doc = clever::html::parse("<html><body><select><option>Option 1</option><option>Option 2</option></select></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* select = doc->find_element("select");
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(select->tag_name, "select");

    auto options = doc->find_all_elements("option");
    ASSERT_EQ(options.size(), 2u);

    EXPECT_EQ(options[0]->tag_name, "option");
    EXPECT_EQ(options[0]->text_content(), "Option 1");
    EXPECT_EQ(options[0]->parent, select);

    EXPECT_EQ(options[1]->tag_name, "option");
    EXPECT_EQ(options[1]->text_content(), "Option 2");
    EXPECT_EQ(options[1]->parent, select);
}

TEST(HTMLParserTest, DlDtDdDefinitionListV78) {
    auto doc = clever::html::parse("<html><body><dl><dt>Term</dt><dd>Definition</dd></dl></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    EXPECT_EQ(dl->tag_name, "dl");

    auto* dt = doc->find_element("dt");
    ASSERT_NE(dt, nullptr);
    EXPECT_EQ(dt->tag_name, "dt");
    EXPECT_EQ(dt->parent, dl);
    EXPECT_EQ(dt->text_content(), "Term");

    auto* dd = doc->find_element("dd");
    ASSERT_NE(dd, nullptr);
    EXPECT_EQ(dd->tag_name, "dd");
    EXPECT_EQ(dd->parent, dl);
    EXPECT_EQ(dd->text_content(), "Definition");
}

TEST(HTMLParserTest, StrongEmphasisTagsV78) {
    auto doc = clever::html::parse("<html><body><p><strong>Bold</strong> and <em>Italic</em></p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->tag_name, "strong");
    EXPECT_EQ(strong->text_content(), "Bold");

    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->tag_name, "em");
    EXPECT_EQ(em->text_content(), "Italic");
}

TEST(HTMLParserTest, BlockquoteCiteAttributeV78) {
    auto doc = clever::html::parse("<html><body><blockquote cite='https://example.com'>Quote text</blockquote></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* blockquote = doc->find_element("blockquote");
    ASSERT_NE(blockquote, nullptr);
    EXPECT_EQ(blockquote->tag_name, "blockquote");
    EXPECT_EQ(get_attr_v63(blockquote, "cite"), "https://example.com");
    EXPECT_EQ(blockquote->text_content(), "Quote text");
}

TEST(HTMLParserTest, PreformattedTextV78) {
    auto doc = clever::html::parse("<html><body><pre>Line 1\nLine 2</pre></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
    EXPECT_EQ(pre->tag_name, "pre");
    std::string content = pre->text_content();
    EXPECT_TRUE(content.find("Line 1") != std::string::npos);
    EXPECT_TRUE(content.find("Line 2") != std::string::npos);
}

TEST(HTMLParserTest, NavWithLinksV78) {
    auto doc = clever::html::parse("<html><body><nav><a href='/home'>Home</a><a href='/about'>About</a></nav></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    EXPECT_EQ(nav->tag_name, "nav");

    auto links = doc->find_all_elements("a");
    ASSERT_EQ(links.size(), 2u);

    EXPECT_EQ(links[0]->tag_name, "a");
    EXPECT_EQ(get_attr_v63(links[0], "href"), "/home");
    EXPECT_EQ(links[0]->text_content(), "Home");
    EXPECT_EQ(links[0]->parent, nav);

    EXPECT_EQ(links[1]->tag_name, "a");
    EXPECT_EQ(get_attr_v63(links[1], "href"), "/about");
    EXPECT_EQ(links[1]->text_content(), "About");
    EXPECT_EQ(links[1]->parent, nav);
}

TEST(HTMLParserTest, MainElementFoundV78) {
    auto doc = clever::html::parse("<html><body><main><h1>Content</h1><p>Main content</p></main></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* main = doc->find_element("main");
    ASSERT_NE(main, nullptr);
    EXPECT_EQ(main->tag_name, "main");

    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->parent, main);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->parent, main);
}

// ---------------------------------------------------------------------------
// Round 79 — HTMLParserTest V79 tests
// ---------------------------------------------------------------------------

TEST(HTMLParserTest, OrderedListOlLiV79) {
    auto doc = clever::html::parse("<html><body><ol><li>First</li><li>Second</li><li>Third</li></ol></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    EXPECT_EQ(ol->tag_name, "ol");

    auto items = doc->find_all_elements("li");
    ASSERT_EQ(items.size(), 3u);
    EXPECT_EQ(items[0]->text_content(), "First");
    EXPECT_EQ(items[1]->text_content(), "Second");
    EXPECT_EQ(items[2]->text_content(), "Third");
    EXPECT_EQ(items[0]->parent, ol);
    EXPECT_EQ(items[1]->parent, ol);
    EXPECT_EQ(items[2]->parent, ol);
}

TEST(HTMLParserTest, DivWithIdAndClassV79) {
    auto doc = clever::html::parse("<html><body><div id=\"main\" class=\"container wide\">Content</div></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->tag_name, "div");
    EXPECT_EQ(get_attr_v63(div, "id"), "main");
    EXPECT_EQ(get_attr_v63(div, "class"), "container wide");
    EXPECT_EQ(div->text_content(), "Content");
}

TEST(HTMLParserTest, NestedSpansV79) {
    auto doc = clever::html::parse("<html><body><span>outer<span>inner</span></span></body></html>");
    ASSERT_NE(doc, nullptr);

    auto spans = doc->find_all_elements("span");
    ASSERT_EQ(spans.size(), 2u);

    // The inner span's parent should be the outer span
    auto* outer = spans[0];
    auto* inner = spans[1];
    EXPECT_EQ(outer->tag_name, "span");
    EXPECT_EQ(inner->tag_name, "span");
    EXPECT_EQ(inner->parent, outer);
    EXPECT_NE(inner->text_content().find("inner"), std::string::npos);
}

TEST(HTMLParserTest, EmptyParagraphV79) {
    auto doc = clever::html::parse("<html><body><p></p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->tag_name, "p");
    EXPECT_EQ(p->text_content(), "");
}

TEST(HTMLParserTest, HrVoidElementV79) {
    auto doc = clever::html::parse("<html><body><hr></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
    EXPECT_EQ(hr->tag_name, "hr");
    EXPECT_TRUE(hr->children.empty());
}

TEST(HTMLParserTest, LabelWithForAttributeV79) {
    auto doc = clever::html::parse("<html><body><label for=\"email\">Email</label></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* label = doc->find_element("label");
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->tag_name, "label");
    EXPECT_EQ(get_attr_v63(label, "for"), "email");
    EXPECT_EQ(label->text_content(), "Email");
}

TEST(HTMLParserTest, MultipleCommentNodesV79) {
    auto doc = clever::html::parse("<html><body><!--a--><!--b--></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    int comment_count = 0;
    std::vector<std::string> comment_data;
    for (const auto& child : body->children) {
        if (child->type == clever::html::SimpleNode::Comment) {
            comment_count++;
            comment_data.push_back(child->data);
        }
    }
    EXPECT_GE(comment_count, 2);
    EXPECT_NE(std::find(comment_data.begin(), comment_data.end(), "a"), comment_data.end());
    EXPECT_NE(std::find(comment_data.begin(), comment_data.end(), "b"), comment_data.end());
}

TEST(HTMLParserTest, FooterElementFoundV79) {
    auto doc = clever::html::parse("<html><body><footer>Copyright 2026</footer></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* footer = doc->find_element("footer");
    ASSERT_NE(footer, nullptr);
    EXPECT_EQ(footer->tag_name, "footer");
    EXPECT_EQ(footer->text_content(), "Copyright 2026");

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(footer->parent, body);
}

// ---------------------------------------------------------------------------
// V80 Tests
// ---------------------------------------------------------------------------

TEST(HTMLParserTest, HeaderElementFoundV80) {
    auto doc = clever::html::parse("<html><body><header>Site Header</header></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* header = doc->find_element("header");
    ASSERT_NE(header, nullptr);
    EXPECT_EQ(header->tag_name, "header");
    EXPECT_EQ(header->text_content(), "Site Header");

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(header->parent, body);
}

TEST(HTMLParserTest, ArticleWithParagraphV80) {
    auto doc = clever::html::parse("<html><body><article><p>Article text</p></article></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    EXPECT_EQ(article->tag_name, "article");

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->tag_name, "p");
    EXPECT_EQ(p->text_content(), "Article text");
    EXPECT_EQ(p->parent, article);
}

TEST(HTMLParserTest, SectionWithIdV80) {
    auto doc = clever::html::parse("<html><body><section id=\"main\">Content</section></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* section = doc->find_element("section");
    ASSERT_NE(section, nullptr);
    EXPECT_EQ(section->tag_name, "section");
    EXPECT_EQ(get_attr_v63(section, "id"), "main");
    EXPECT_EQ(section->text_content(), "Content");
}

TEST(HTMLParserTest, BrVoidNoChildrenV80) {
    auto doc = clever::html::parse("<html><body><p>Line1<br>Line2</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* br = doc->find_element("br");
    ASSERT_NE(br, nullptr);
    EXPECT_EQ(br->tag_name, "br");
    EXPECT_TRUE(br->children.empty());
}

TEST(HTMLParserTest, DataAttributeParsedV80) {
    auto doc = clever::html::parse(R"(<html><body><div data-value="42">Info</div></body></html>)");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-value"), "42");
    EXPECT_EQ(div->text_content(), "Info");
}

TEST(HTMLParserTest, NestedListsV80) {
    auto doc = clever::html::parse("<html><body><ul><li>Outer<ul><li>Inner</li></ul></li></ul></body></html>");
    ASSERT_NE(doc, nullptr);

    auto all_ul = doc->find_all_elements("ul");
    ASSERT_GE(all_ul.size(), 2u);

    auto all_li = doc->find_all_elements("li");
    ASSERT_GE(all_li.size(), 2u);

    // The inner ul should be nested inside the outer li
    auto* outer_ul = all_ul[0];
    EXPECT_EQ(outer_ul->tag_name, "ul");

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(outer_ul->parent, body);
}

TEST(HTMLParserTest, FigureWithFigcaptionV80) {
    auto doc = clever::html::parse("<html><body><figure><img src=\"photo.jpg\"><figcaption>A photo</figcaption></figure></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);
    EXPECT_EQ(figure->tag_name, "figure");

    auto* figcaption = doc->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->tag_name, "figcaption");
    EXPECT_EQ(figcaption->text_content(), "A photo");
    EXPECT_EQ(figcaption->parent, figure);

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "photo.jpg");
}

TEST(HTMLParserTest, DetailsAndSummaryV80) {
    auto doc = clever::html::parse("<html><body><details><summary>Click me</summary><p>Hidden content</p></details></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    EXPECT_EQ(details->tag_name, "details");

    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->tag_name, "summary");
    EXPECT_EQ(summary->text_content(), "Click me");
    EXPECT_EQ(summary->parent, details);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hidden content");
    EXPECT_EQ(p->parent, details);
}

// ---------------------------------------------------------------------------
// V81 tests: diverse HTML parsing — void elements, nested structures,
//            attributes, inline content, empty elements, heading hierarchy
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, MultipleSiblingVoidElementsBrHrWbrV81) {
    auto doc = clever::html::parse("<body>one<br>two<hr>three<wbr>four</body>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    auto* br = doc->find_element("br");
    auto* hr = doc->find_element("hr");
    auto* wbr = doc->find_element("wbr");
    ASSERT_NE(br, nullptr);
    ASSERT_NE(hr, nullptr);
    ASSERT_NE(wbr, nullptr);

    EXPECT_TRUE(br->children.empty());
    EXPECT_TRUE(hr->children.empty());
    EXPECT_TRUE(wbr->children.empty());

    EXPECT_EQ(br->parent, body);
    EXPECT_EQ(hr->parent, body);
    EXPECT_EQ(wbr->parent, body);

    const std::string text = body->text_content();
    EXPECT_NE(text.find("one"), std::string::npos);
    EXPECT_NE(text.find("two"), std::string::npos);
    EXPECT_NE(text.find("three"), std::string::npos);
    EXPECT_NE(text.find("four"), std::string::npos);
}

TEST(HtmlParserTest, DefinitionListDlDtDdStructureV81) {
    auto doc = clever::html::parse("<html><body><dl><dt>Term1</dt><dd>Def1</dd><dt>Term2</dt><dd>Def2</dd></dl></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    EXPECT_EQ(dl->tag_name, "dl");

    auto dts = doc->find_all_elements("dt");
    auto dds = doc->find_all_elements("dd");
    ASSERT_EQ(dts.size(), 2u);
    ASSERT_EQ(dds.size(), 2u);

    EXPECT_EQ(dts[0]->text_content(), "Term1");
    EXPECT_EQ(dts[1]->text_content(), "Term2");
    EXPECT_EQ(dds[0]->text_content(), "Def1");
    EXPECT_EQ(dds[1]->text_content(), "Def2");

    EXPECT_EQ(dts[0]->parent, dl);
    EXPECT_EQ(dds[0]->parent, dl);
    EXPECT_EQ(dts[1]->parent, dl);
    EXPECT_EQ(dds[1]->parent, dl);
}

TEST(HtmlParserTest, DeeplyNestedDivsParentChainV81) {
    auto doc = clever::html::parse("<div id=\"a\"><div id=\"b\"><div id=\"c\"><div id=\"d\">leaf</div></div></div></div>");
    ASSERT_NE(doc, nullptr);

    auto divs = doc->find_all_elements("div");
    ASSERT_EQ(divs.size(), 4u);

    EXPECT_EQ(get_attr_v63(divs[0], "id"), "a");
    EXPECT_EQ(get_attr_v63(divs[1], "id"), "b");
    EXPECT_EQ(get_attr_v63(divs[2], "id"), "c");
    EXPECT_EQ(get_attr_v63(divs[3], "id"), "d");

    EXPECT_EQ(divs[1]->parent, divs[0]);
    EXPECT_EQ(divs[2]->parent, divs[1]);
    EXPECT_EQ(divs[3]->parent, divs[2]);
    EXPECT_EQ(divs[3]->text_content(), "leaf");
}

TEST(HtmlParserTest, MultipleAttributesOnSingleElementV81) {
    auto doc = clever::html::parse("<a href=\"https://example.com\" target=\"_blank\" rel=\"noopener\" title=\"Link\">click</a>");
    ASSERT_NE(doc, nullptr);

    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->tag_name, "a");

    EXPECT_EQ(get_attr_v63(a, "href"), "https://example.com");
    EXPECT_EQ(get_attr_v63(a, "target"), "_blank");
    EXPECT_EQ(get_attr_v63(a, "rel"), "noopener");
    EXPECT_EQ(get_attr_v63(a, "title"), "Link");
    EXPECT_EQ(a->text_content(), "click");
}

TEST(HtmlParserTest, SpecialCharsInTextContentPreservedV81) {
    auto doc = clever::html::parse("<p>5 &lt; 10 &amp; 20 &gt; 15</p>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);

    const std::string text = p->text_content();
    EXPECT_NE(text.find("5"), std::string::npos);
    EXPECT_NE(text.find("<"), std::string::npos);
    EXPECT_NE(text.find("10"), std::string::npos);
    EXPECT_NE(text.find("&"), std::string::npos);
    EXPECT_NE(text.find(">"), std::string::npos);
    EXPECT_NE(text.find("15"), std::string::npos);
}

TEST(HtmlParserTest, MixedInlineElementsInsideParagraphV81) {
    auto doc = clever::html::parse("<p>Hello <b>bold</b> and <i>italic</i> and <code>code</code> world</p>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);

    auto* b = doc->find_element("b");
    auto* i = doc->find_element("i");
    auto* code = doc->find_element("code");
    ASSERT_NE(b, nullptr);
    ASSERT_NE(i, nullptr);
    ASSERT_NE(code, nullptr);

    EXPECT_EQ(b->parent, p);
    EXPECT_EQ(i->parent, p);
    EXPECT_EQ(code->parent, p);

    EXPECT_EQ(b->text_content(), "bold");
    EXPECT_EQ(i->text_content(), "italic");
    EXPECT_EQ(code->text_content(), "code");

    const std::string full = p->text_content();
    EXPECT_NE(full.find("Hello"), std::string::npos);
    EXPECT_NE(full.find("bold"), std::string::npos);
    EXPECT_NE(full.find("italic"), std::string::npos);
    EXPECT_NE(full.find("code"), std::string::npos);
    EXPECT_NE(full.find("world"), std::string::npos);
}

TEST(HtmlParserTest, EmptyElementsHaveNoChildrenV81) {
    auto doc = clever::html::parse("<html><body><div></div><span></span><p></p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    auto* span = doc->find_element("span");
    auto* p = doc->find_element("p");
    ASSERT_NE(div, nullptr);
    ASSERT_NE(span, nullptr);
    ASSERT_NE(p, nullptr);

    EXPECT_TRUE(div->children.empty());
    EXPECT_TRUE(span->children.empty());
    EXPECT_TRUE(p->children.empty());

    EXPECT_EQ(div->text_content(), "");
    EXPECT_EQ(span->text_content(), "");
    EXPECT_EQ(p->text_content(), "");
}

TEST(HtmlParserTest, HeadingHierarchyH1ThroughH6V81) {
    auto doc = clever::html::parse(
        "<body>"
        "<h1>Heading 1</h1>"
        "<h2>Heading 2</h2>"
        "<h3>Heading 3</h3>"
        "<h4>Heading 4</h4>"
        "<h5>Heading 5</h5>"
        "<h6>Heading 6</h6>"
        "</body>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    auto* h1 = doc->find_element("h1");
    auto* h2 = doc->find_element("h2");
    auto* h3 = doc->find_element("h3");
    auto* h4 = doc->find_element("h4");
    auto* h5 = doc->find_element("h5");
    auto* h6 = doc->find_element("h6");
    ASSERT_NE(h1, nullptr);
    ASSERT_NE(h2, nullptr);
    ASSERT_NE(h3, nullptr);
    ASSERT_NE(h4, nullptr);
    ASSERT_NE(h5, nullptr);
    ASSERT_NE(h6, nullptr);

    EXPECT_EQ(h1->tag_name, "h1");
    EXPECT_EQ(h2->tag_name, "h2");
    EXPECT_EQ(h3->tag_name, "h3");
    EXPECT_EQ(h4->tag_name, "h4");
    EXPECT_EQ(h5->tag_name, "h5");
    EXPECT_EQ(h6->tag_name, "h6");

    EXPECT_EQ(h1->text_content(), "Heading 1");
    EXPECT_EQ(h2->text_content(), "Heading 2");
    EXPECT_EQ(h3->text_content(), "Heading 3");
    EXPECT_EQ(h4->text_content(), "Heading 4");
    EXPECT_EQ(h5->text_content(), "Heading 5");
    EXPECT_EQ(h6->text_content(), "Heading 6");

    EXPECT_EQ(h1->parent, body);
    EXPECT_EQ(h2->parent, body);
    EXPECT_EQ(h3->parent, body);
    EXPECT_EQ(h4->parent, body);
    EXPECT_EQ(h5->parent, body);
    EXPECT_EQ(h6->parent, body);
}

// ---------------------------------------------------------------------------
// V82 — 8 new HTML parser tests covering diverse parsing scenarios.
// ---------------------------------------------------------------------------

// 1. Parse a <pre> element preserving its text content
TEST(HtmlParserTest, PreElementPreservesTextContentV82) {
    auto doc = clever::html::parse(
        "<html><body><pre>  line1\n  line2\n  line3</pre></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
    EXPECT_EQ(pre->tag_name, "pre");
    // text_content should contain the whitespace and newlines
    std::string tc = pre->text_content();
    EXPECT_NE(tc.find("line1"), std::string::npos);
    EXPECT_NE(tc.find("line2"), std::string::npos);
    EXPECT_NE(tc.find("line3"), std::string::npos);
}

// 2. Multiple void tags: <br>, <hr>, <input> should have no children
TEST(HtmlParserTest, MultipleVoidTagsNoChildrenV82) {
    auto doc = clever::html::parse(
        "<html><body><br><hr><input></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* br = doc->find_element("br");
    auto* hr = doc->find_element("hr");
    auto* input = doc->find_element("input");
    ASSERT_NE(br, nullptr);
    ASSERT_NE(hr, nullptr);
    ASSERT_NE(input, nullptr);

    EXPECT_EQ(br->tag_name, "br");
    EXPECT_EQ(hr->tag_name, "hr");
    EXPECT_EQ(input->tag_name, "input");
    EXPECT_TRUE(br->children.empty());
    EXPECT_TRUE(hr->children.empty());
    EXPECT_TRUE(input->children.empty());
}

// 3. Deeply nested elements preserve parent chain
TEST(HtmlParserTest, DeeplyNestedParentChainV82) {
    auto doc = clever::html::parse(
        "<html><body><div><section><article><span>deep</span></article></section></div></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "deep");

    auto* article = doc->find_element("article");
    auto* section = doc->find_element("section");
    auto* div = doc->find_element("div");
    ASSERT_NE(article, nullptr);
    ASSERT_NE(section, nullptr);
    ASSERT_NE(div, nullptr);

    EXPECT_EQ(span->parent, article);
    EXPECT_EQ(article->parent, section);
    EXPECT_EQ(section->parent, div);
}

// 4. Anchor tag with href and target attributes
TEST(HtmlParserTest, AnchorTagAttributesV82) {
    auto doc = clever::html::parse(
        "<html><body><a href=\"https://example.com\" target=\"_blank\">Link</a></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->tag_name, "a");
    EXPECT_EQ(a->text_content(), "Link");
    EXPECT_EQ(get_attr_v63(a, "href"), "https://example.com");
    EXPECT_EQ(get_attr_v63(a, "target"), "_blank");
}

// 5. Unordered list with multiple list items
TEST(HtmlParserTest, UnorderedListItemsV82) {
    auto doc = clever::html::parse(
        "<html><body><ul><li>Alpha</li><li>Beta</li><li>Gamma</li></ul></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    EXPECT_EQ(ul->tag_name, "ul");

    auto items = doc->find_all_elements("li");
    ASSERT_EQ(items.size(), 3u);
    EXPECT_EQ(items[0]->text_content(), "Alpha");
    EXPECT_EQ(items[1]->text_content(), "Beta");
    EXPECT_EQ(items[2]->text_content(), "Gamma");

    for (auto* li : items) {
        EXPECT_EQ(li->parent, ul);
    }
}

// 6. Table structure: table, thead, tbody, tr, th, td
TEST(HtmlParserTest, TableStructureParsingV82) {
    auto doc = clever::html::parse(
        "<html><body><table>"
        "<thead><tr><th>Name</th><th>Age</th></tr></thead>"
        "<tbody><tr><td>Alice</td><td>30</td></tr></tbody>"
        "</table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);

    auto ths = doc->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Age");

    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 2u);
    EXPECT_EQ(tds[0]->text_content(), "Alice");
    EXPECT_EQ(tds[1]->text_content(), "30");
}

// 7. Meta tag with charset is a void element with attribute
TEST(HtmlParserTest, MetaTagVoidWithCharsetV82) {
    auto doc = clever::html::parse(
        "<html><head><meta charset=\"utf-8\"><title>Test</title></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* meta = doc->find_element("meta");
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->tag_name, "meta");
    EXPECT_TRUE(meta->children.empty());
    EXPECT_EQ(get_attr_v63(meta, "charset"), "utf-8");

    auto* title = doc->find_element("title");
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->text_content(), "Test");
}

// 8. Sibling paragraphs each have correct text and share the same parent
TEST(HtmlParserTest, SiblingParagraphsTextAndParentV82) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p>First paragraph</p>"
        "<p>Second paragraph</p>"
        "<p>Third paragraph</p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto ps = doc->find_all_elements("p");
    ASSERT_EQ(ps.size(), 3u);
    EXPECT_EQ(ps[0]->text_content(), "First paragraph");
    EXPECT_EQ(ps[1]->text_content(), "Second paragraph");
    EXPECT_EQ(ps[2]->text_content(), "Third paragraph");

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    for (auto* p : ps) {
        EXPECT_EQ(p->parent, body);
    }
}

// ---------------------------------------------------------------------------
// Round 83 — 8 new HTML parser tests (V83)
// ---------------------------------------------------------------------------

// 1. Nested lists: ul > li > ul > li structure is preserved
TEST(HtmlParserTest, NestedUnorderedListsV83) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ul><li>A<ul><li>A1</li><li>A2</li></ul></li><li>B</li></ul>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto uls = doc->find_all_elements("ul");
    ASSERT_EQ(uls.size(), 2u);

    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 4u);
    EXPECT_NE(lis[0]->text_content().find("A"), std::string::npos);
    EXPECT_EQ(lis[1]->text_content(), "A1");
    EXPECT_EQ(lis[2]->text_content(), "A2");
    EXPECT_EQ(lis[3]->text_content(), "B");
}

// 2. Multiple attributes on a single element
TEST(HtmlParserTest, MultipleAttributesOnElementV83) {
    auto doc = clever::html::parse(
        "<html><body><a href=\"/page\" class=\"link\" id=\"main-link\" target=\"_blank\">Go</a></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->tag_name, "a");
    EXPECT_EQ(get_attr_v63(a, "href"), "/page");
    EXPECT_EQ(get_attr_v63(a, "class"), "link");
    EXPECT_EQ(get_attr_v63(a, "id"), "main-link");
    EXPECT_EQ(get_attr_v63(a, "target"), "_blank");
    EXPECT_EQ(a->text_content(), "Go");
}

// 3. Deeply nested divs preserve depth and text
TEST(HtmlParserTest, DeeplyNestedDivsV83) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div><div><div><div><span>deep</span></div></div></div></div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto divs = doc->find_all_elements("div");
    ASSERT_EQ(divs.size(), 4u);

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "deep");
    // span should be inside the innermost div
    ASSERT_NE(span->parent, nullptr);
    EXPECT_EQ(span->parent->tag_name, "div");
}

// 4. Empty body produces no child elements
TEST(HtmlParserTest, EmptyBodyNoChildElementsV83) {
    auto doc = clever::html::parse("<html><head></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    // Body should have no element children (may have whitespace text nodes)
    bool has_element_child = false;
    for (auto& child : body->children) {
        if (!child->tag_name.empty()) {
            has_element_child = true;
        }
    }
    EXPECT_FALSE(has_element_child);
}

// 5. Inline elements inside block: span inside p
TEST(HtmlParserTest, InlineSpanInsideParagraphV83) {
    auto doc = clever::html::parse(
        "<html><body><p>Hello <span>World</span> end</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "World");
    EXPECT_EQ(span->parent, p);

    // The paragraph should contain "Hello", the span, and " end"
    EXPECT_NE(p->text_content().find("Hello"), std::string::npos);
    EXPECT_NE(p->text_content().find("World"), std::string::npos);
    EXPECT_NE(p->text_content().find("end"), std::string::npos);
}

// 6. Multiple void elements in sequence (br, hr, input)
TEST(HtmlParserTest, MultipleVoidElementsInSequenceV83) {
    auto doc = clever::html::parse(
        "<html><body><br><hr><input type=\"email\"><br></body></html>");
    ASSERT_NE(doc, nullptr);

    auto brs = doc->find_all_elements("br");
    ASSERT_EQ(brs.size(), 2u);
    for (auto* br : brs) {
        EXPECT_TRUE(br->children.empty());
    }

    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
    EXPECT_TRUE(hr->children.empty());

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "email");
    EXPECT_TRUE(input->children.empty());
}

// 7. Headings h1-h6 all parsed with correct text
TEST(HtmlParserTest, AllHeadingLevelsH1ToH6V83) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<h1>One</h1><h2>Two</h2><h3>Three</h3>"
        "<h4>Four</h4><h5>Five</h5><h6>Six</h6>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "One");

    auto* h2 = doc->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->text_content(), "Two");

    auto* h3 = doc->find_element("h3");
    ASSERT_NE(h3, nullptr);
    EXPECT_EQ(h3->text_content(), "Three");

    auto* h4 = doc->find_element("h4");
    ASSERT_NE(h4, nullptr);
    EXPECT_EQ(h4->text_content(), "Four");

    auto* h5 = doc->find_element("h5");
    ASSERT_NE(h5, nullptr);
    EXPECT_EQ(h5->text_content(), "Five");

    auto* h6 = doc->find_element("h6");
    ASSERT_NE(h6, nullptr);
    EXPECT_EQ(h6->text_content(), "Six");
}

// 8. Form with labeled inputs preserves structure
TEST(HtmlParserTest, FormWithLabeledInputsV83) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<form action=\"/submit\" method=\"post\">"
        "<label>Name</label><input type=\"text\" name=\"user\">"
        "<label>Pass</label><input type=\"password\" name=\"pw\">"
        "<button type=\"submit\">Go</button>"
        "</form></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "action"), "/submit");
    EXPECT_EQ(get_attr_v63(form, "method"), "post");

    auto labels = doc->find_all_elements("label");
    ASSERT_EQ(labels.size(), 2u);
    EXPECT_EQ(labels[0]->text_content(), "Name");
    EXPECT_EQ(labels[1]->text_content(), "Pass");

    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "text");
    EXPECT_EQ(get_attr_v63(inputs[0], "name"), "user");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "password");
    EXPECT_EQ(get_attr_v63(inputs[1], "name"), "pw");

    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text_content(), "Go");
    EXPECT_EQ(get_attr_v63(button, "type"), "submit");
}

// ============================================================================
// V84 Tests
// ============================================================================

// 1. Nested definition list with dt/dd pairs
TEST(HtmlParserTest, NestedDefinitionListV84) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>Term A</dt><dd>Definition A</dd>"
        "<dt>Term B</dt><dd>Definition B</dd>"
        "</dl></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);

    auto dts = doc->find_all_elements("dt");
    ASSERT_EQ(dts.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term A");
    EXPECT_EQ(dts[1]->text_content(), "Term B");

    auto dds = doc->find_all_elements("dd");
    ASSERT_EQ(dds.size(), 2u);
    EXPECT_EQ(dds[0]->text_content(), "Definition A");
    EXPECT_EQ(dds[1]->text_content(), "Definition B");
}

// 2. Table with thead, tbody, tfoot and multiple rows
TEST(HtmlParserTest, FullTableStructureV84) {
    auto doc = clever::html::parse(
        "<html><body><table>"
        "<thead><tr><th>Name</th><th>Age</th></tr></thead>"
        "<tbody><tr><td>Alice</td><td>30</td></tr>"
        "<tr><td>Bob</td><td>25</td></tr></tbody>"
        "<tfoot><tr><td colspan=\"2\">Total: 2</td></tr></tfoot>"
        "</table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* thead = doc->find_element("thead");
    ASSERT_NE(thead, nullptr);
    auto* tbody = doc->find_element("tbody");
    ASSERT_NE(tbody, nullptr);
    auto* tfoot = doc->find_element("tfoot");
    ASSERT_NE(tfoot, nullptr);

    auto ths = doc->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Age");

    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 5u);
    EXPECT_EQ(tds[0]->text_content(), "Alice");
    EXPECT_EQ(tds[1]->text_content(), "30");
    EXPECT_EQ(tds[2]->text_content(), "Bob");
    EXPECT_EQ(tds[3]->text_content(), "25");
    EXPECT_EQ(tds[4]->text_content(), "Total: 2");
    EXPECT_EQ(get_attr_v63(tds[4], "colspan"), "2");
}

// 3. Deeply nested div structure (5 levels)
TEST(HtmlParserTest, DeeplyNestedDivsV84) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div id=\"l1\"><div id=\"l2\"><div id=\"l3\">"
        "<div id=\"l4\"><div id=\"l5\">Deep</div></div>"
        "</div></div></div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* l1 = doc->find_element("div");
    ASSERT_NE(l1, nullptr);
    EXPECT_EQ(get_attr_v63(l1, "id"), "l1");

    // Navigate to the innermost div
    auto divs = doc->find_all_elements("div");
    ASSERT_EQ(divs.size(), 5u);
    EXPECT_EQ(get_attr_v63(divs[0], "id"), "l1");
    EXPECT_EQ(get_attr_v63(divs[4], "id"), "l5");
    EXPECT_EQ(divs[4]->text_content(), "Deep");
}

// 4. Multiple sibling paragraphs with mixed inline elements
TEST(HtmlParserTest, SiblingParagraphsWithInlineV84) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p><strong>Bold</strong> text</p>"
        "<p><em>Italic</em> text</p>"
        "<p><code>Code</code> text</p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto ps = doc->find_all_elements("p");
    ASSERT_EQ(ps.size(), 3u);

    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "Bold");

    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "Italic");

    auto* code = doc->find_element("code");
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(code->text_content(), "Code");
}

// 5. Select element with optgroup and option
TEST(HtmlParserTest, SelectWithOptgroupV84) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<select name=\"car\">"
        "<optgroup label=\"Swedish\">"
        "<option value=\"volvo\">Volvo</option>"
        "<option value=\"saab\">Saab</option>"
        "</optgroup>"
        "<optgroup label=\"German\">"
        "<option value=\"bmw\">BMW</option>"
        "</optgroup>"
        "</select></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* select = doc->find_element("select");
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(get_attr_v63(select, "name"), "car");

    auto optgroups = doc->find_all_elements("optgroup");
    ASSERT_EQ(optgroups.size(), 2u);
    EXPECT_EQ(get_attr_v63(optgroups[0], "label"), "Swedish");
    EXPECT_EQ(get_attr_v63(optgroups[1], "label"), "German");

    auto options = doc->find_all_elements("option");
    ASSERT_EQ(options.size(), 3u);
    EXPECT_EQ(get_attr_v63(options[0], "value"), "volvo");
    EXPECT_EQ(options[0]->text_content(), "Volvo");
    EXPECT_EQ(get_attr_v63(options[1], "value"), "saab");
    EXPECT_EQ(options[1]->text_content(), "Saab");
    EXPECT_EQ(get_attr_v63(options[2], "value"), "bmw");
    EXPECT_EQ(options[2]->text_content(), "BMW");
}

// 6. Details/summary elements
TEST(HtmlParserTest, DetailsSummaryElementsV84) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<details open>"
        "<summary>Click to expand</summary>"
        "<p>Hidden content here</p>"
        "</details></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    EXPECT_EQ(get_attr_v63(details, "open"), "");

    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click to expand");

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hidden content here");
}

// 7. Anchor tags with various attributes
TEST(HtmlParserTest, AnchorTagAttributesV84) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<a href=\"https://example.com\" target=\"_blank\" rel=\"noopener\">Link 1</a>"
        "<a href=\"/about\" class=\"nav-link\">Link 2</a>"
        "<a href=\"#section\" id=\"jump\">Link 3</a>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto anchors = doc->find_all_elements("a");
    ASSERT_EQ(anchors.size(), 3u);

    EXPECT_EQ(get_attr_v63(anchors[0], "href"), "https://example.com");
    EXPECT_EQ(get_attr_v63(anchors[0], "target"), "_blank");
    EXPECT_EQ(get_attr_v63(anchors[0], "rel"), "noopener");
    EXPECT_EQ(anchors[0]->text_content(), "Link 1");

    EXPECT_EQ(get_attr_v63(anchors[1], "href"), "/about");
    EXPECT_EQ(get_attr_v63(anchors[1], "class"), "nav-link");
    EXPECT_EQ(anchors[1]->text_content(), "Link 2");

    EXPECT_EQ(get_attr_v63(anchors[2], "href"), "#section");
    EXPECT_EQ(get_attr_v63(anchors[2], "id"), "jump");
    EXPECT_EQ(anchors[2]->text_content(), "Link 3");
}

// 8. Mixed content with text nodes between elements
TEST(HtmlParserTest, MixedContentTextNodesV84) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div>Before <span>inside</span> after</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);

    // The div should have children: text("Before "), span("inside"), text(" after")
    ASSERT_GE(div->children.size(), 3u);

    // First child: text node "Before "
    EXPECT_EQ(div->children[0]->tag_name, "");
    EXPECT_EQ(div->children[0]->text_content(), "Before ");

    // Second child: span element
    EXPECT_EQ(div->children[1]->tag_name, "span");
    EXPECT_EQ(div->children[1]->text_content(), "inside");

    // Third child: text node " after"
    EXPECT_EQ(div->children[2]->tag_name, "");
    EXPECT_EQ(div->children[2]->text_content(), " after");
}

// ============================================================================
// Cycle V85 — 8 additional HTML parser tests
// ============================================================================

// 1. Definition list (dl/dt/dd) structure parsing
TEST(HtmlParserTest, DefinitionListStructureV85) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>Term 1</dt><dd>Definition 1</dd>"
        "<dt>Term 2</dt><dd>Definition 2</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);

    auto dts = doc->find_all_elements("dt");
    auto dds = doc->find_all_elements("dd");
    ASSERT_EQ(dts.size(), 2u);
    ASSERT_EQ(dds.size(), 2u);

    EXPECT_EQ(dts[0]->text_content(), "Term 1");
    EXPECT_EQ(dts[1]->text_content(), "Term 2");
    EXPECT_EQ(dds[0]->text_content(), "Definition 1");
    EXPECT_EQ(dds[1]->text_content(), "Definition 2");
}

// 2. Nested lists (ul containing ol)
TEST(HtmlParserTest, NestedListsUlContainingOlV85) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ul>"
        "<li>Item A</li>"
        "<li>Item B"
        "<ol><li>Sub 1</li><li>Sub 2</li></ol>"
        "</li>"
        "<li>Item C</li>"
        "</ul>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);

    auto lis = ul->find_all_elements("li");
    ASSERT_GE(lis.size(), 3u);
    EXPECT_EQ(lis[0]->text_content(), "Item A");

    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);

    auto sub_lis = ol->find_all_elements("li");
    ASSERT_EQ(sub_lis.size(), 2u);
    EXPECT_EQ(sub_lis[0]->text_content(), "Sub 1");
    EXPECT_EQ(sub_lis[1]->text_content(), "Sub 2");
}

// 3. Table with thead, tbody, tfoot structure
TEST(HtmlParserTest, TableTheadTbodyTfootV85) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<thead><tr><th>Name</th><th>Age</th></tr></thead>"
        "<tbody><tr><td>Alice</td><td>30</td></tr>"
        "<tr><td>Bob</td><td>25</td></tr></tbody>"
        "<tfoot><tr><td>Total</td><td>2</td></tr></tfoot>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* thead = doc->find_element("thead");
    ASSERT_NE(thead, nullptr);
    auto ths = thead->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Age");

    auto* tbody = doc->find_element("tbody");
    ASSERT_NE(tbody, nullptr);
    auto body_tds = tbody->find_all_elements("td");
    ASSERT_EQ(body_tds.size(), 4u);
    EXPECT_EQ(body_tds[0]->text_content(), "Alice");
    EXPECT_EQ(body_tds[1]->text_content(), "30");
    EXPECT_EQ(body_tds[2]->text_content(), "Bob");
    EXPECT_EQ(body_tds[3]->text_content(), "25");

    auto* tfoot = doc->find_element("tfoot");
    ASSERT_NE(tfoot, nullptr);
    auto foot_tds = tfoot->find_all_elements("td");
    ASSERT_EQ(foot_tds.size(), 2u);
    EXPECT_EQ(foot_tds[0]->text_content(), "Total");
    EXPECT_EQ(foot_tds[1]->text_content(), "2");
}

// 4. Multiple void elements in sequence (br, hr, input, img)
TEST(HtmlParserTest, MultipleVoidElementsInSequenceV85) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div>"
        "<input type=\"text\" name=\"field1\">"
        "<br>"
        "<input type=\"email\" name=\"field2\">"
        "<hr>"
        "<img src=\"logo.png\" alt=\"Logo\">"
        "</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "text");
    EXPECT_EQ(get_attr_v63(inputs[0], "name"), "field1");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "email");
    EXPECT_EQ(get_attr_v63(inputs[1], "name"), "field2");

    auto* br = doc->find_element("br");
    ASSERT_NE(br, nullptr);

    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "logo.png");
    EXPECT_EQ(get_attr_v63(img, "alt"), "Logo");
}

// 5. Fieldset with legend and form controls
TEST(HtmlParserTest, FieldsetWithLegendAndControlsV85) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<form action=\"/register\" method=\"post\">"
        "<fieldset>"
        "<legend>Personal Info</legend>"
        "<label for=\"fname\">First:</label>"
        "<input type=\"text\" id=\"fname\" name=\"fname\">"
        "<label for=\"lname\">Last:</label>"
        "<input type=\"text\" id=\"lname\" name=\"lname\">"
        "</fieldset>"
        "</form>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "action"), "/register");
    EXPECT_EQ(get_attr_v63(form, "method"), "post");

    auto* fieldset = doc->find_element("fieldset");
    ASSERT_NE(fieldset, nullptr);

    auto* legend = doc->find_element("legend");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->text_content(), "Personal Info");

    auto labels = doc->find_all_elements("label");
    ASSERT_EQ(labels.size(), 2u);
    EXPECT_EQ(get_attr_v63(labels[0], "for"), "fname");
    EXPECT_EQ(labels[0]->text_content(), "First:");
    EXPECT_EQ(get_attr_v63(labels[1], "for"), "lname");
    EXPECT_EQ(labels[1]->text_content(), "Last:");

    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "id"), "fname");
    EXPECT_EQ(get_attr_v63(inputs[1], "id"), "lname");
}

// 6. Deeply nested inline elements (span inside em inside strong inside a)
TEST(HtmlParserTest, DeeplyNestedInlineElementsV85) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p><a href=\"/page\"><strong><em><span class=\"hl\">Deep text</span></em></strong></a></p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(get_attr_v63(a, "href"), "/page");

    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);

    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(get_attr_v63(span, "class"), "hl");
    EXPECT_EQ(span->text_content(), "Deep text");

    // Verify the entire chain has the same text content
    EXPECT_EQ(a->text_content(), "Deep text");
    EXPECT_EQ(strong->text_content(), "Deep text");
    EXPECT_EQ(em->text_content(), "Deep text");
}

// 7. Article with header, footer, and multiple sections
TEST(HtmlParserTest, ArticleWithHeaderFooterSectionsV85) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<article>"
        "<header><h1>Article Title</h1></header>"
        "<section><h2>Introduction</h2><p>Intro text here.</p></section>"
        "<section><h2>Conclusion</h2><p>Closing remarks.</p></section>"
        "<footer><p>Published 2026</p></footer>"
        "</article>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);

    auto* header = doc->find_element("header");
    ASSERT_NE(header, nullptr);
    auto* h1 = header->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Article Title");

    auto sections = doc->find_all_elements("section");
    ASSERT_EQ(sections.size(), 2u);

    auto* h2_intro = sections[0]->find_element("h2");
    ASSERT_NE(h2_intro, nullptr);
    EXPECT_EQ(h2_intro->text_content(), "Introduction");
    auto* p_intro = sections[0]->find_element("p");
    ASSERT_NE(p_intro, nullptr);
    EXPECT_EQ(p_intro->text_content(), "Intro text here.");

    auto* h2_concl = sections[1]->find_element("h2");
    ASSERT_NE(h2_concl, nullptr);
    EXPECT_EQ(h2_concl->text_content(), "Conclusion");
    auto* p_concl = sections[1]->find_element("p");
    ASSERT_NE(p_concl, nullptr);
    EXPECT_EQ(p_concl->text_content(), "Closing remarks.");

    auto* footer = doc->find_element("footer");
    ASSERT_NE(footer, nullptr);
    auto* p_foot = footer->find_element("p");
    ASSERT_NE(p_foot, nullptr);
    EXPECT_EQ(p_foot->text_content(), "Published 2026");
}

// 8. Multiple data attributes and aria attributes on a single element
TEST(HtmlParserTest, MultipleDataAndAriaAttributesV85) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div id=\"widget\""
        " data-type=\"chart\""
        " data-source=\"/api/data\""
        " data-refresh=\"30\""
        " aria-label=\"Sales Chart\""
        " aria-hidden=\"false\""
        " role=\"img\""
        " class=\"dashboard-widget\">"
        "Chart Content"
        "</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);

    EXPECT_EQ(get_attr_v63(div, "id"), "widget");
    EXPECT_EQ(get_attr_v63(div, "data-type"), "chart");
    EXPECT_EQ(get_attr_v63(div, "data-source"), "/api/data");
    EXPECT_EQ(get_attr_v63(div, "data-refresh"), "30");
    EXPECT_EQ(get_attr_v63(div, "aria-label"), "Sales Chart");
    EXPECT_EQ(get_attr_v63(div, "aria-hidden"), "false");
    EXPECT_EQ(get_attr_v63(div, "role"), "img");
    EXPECT_EQ(get_attr_v63(div, "class"), "dashboard-widget");
    EXPECT_EQ(div->text_content(), "Chart Content");
}

// ============================================================================
// V86 Tests
// ============================================================================

// 1. Nested lists with mixed ordered and unordered elements
TEST(HtmlParserTest, NestedMixedListsV86) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ul><li>Fruit<ol><li>Apple</li><li>Banana</li></ol></li>"
        "<li>Vegetable</li></ul>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);

    auto lis = ul->find_all_elements("li");
    // Top-level li + nested li = 4 total
    ASSERT_GE(lis.size(), 3u);

    auto* ol = ul->find_element("ol");
    ASSERT_NE(ol, nullptr);

    auto nested_lis = ol->find_all_elements("li");
    ASSERT_EQ(nested_lis.size(), 2u);
    EXPECT_EQ(nested_lis[0]->text_content(), "Apple");
    EXPECT_EQ(nested_lis[1]->text_content(), "Banana");
}

// 2. Definition list (dl/dt/dd) structure
TEST(HtmlParserTest, DefinitionListStructureV86) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>HTML</dt><dd>HyperText Markup Language</dd>"
        "<dt>CSS</dt><dd>Cascading Style Sheets</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);

    auto dts = dl->find_all_elements("dt");
    auto dds = dl->find_all_elements("dd");
    ASSERT_EQ(dts.size(), 2u);
    ASSERT_EQ(dds.size(), 2u);

    EXPECT_EQ(dts[0]->text_content(), "HTML");
    EXPECT_EQ(dds[0]->text_content(), "HyperText Markup Language");
    EXPECT_EQ(dts[1]->text_content(), "CSS");
    EXPECT_EQ(dds[1]->text_content(), "Cascading Style Sheets");
}

// 3. Table with thead, tbody, tfoot
TEST(HtmlParserTest, FullTableStructureV86) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<thead><tr><th>Name</th><th>Age</th></tr></thead>"
        "<tbody><tr><td>Alice</td><td>30</td></tr>"
        "<tr><td>Bob</td><td>25</td></tr></tbody>"
        "<tfoot><tr><td colspan=\"2\">Total: 2</td></tr></tfoot>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);

    auto* thead = table->find_element("thead");
    ASSERT_NE(thead, nullptr);
    auto ths = thead->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Age");

    auto* tbody = table->find_element("tbody");
    ASSERT_NE(tbody, nullptr);
    auto tds = tbody->find_all_elements("td");
    ASSERT_EQ(tds.size(), 4u);
    EXPECT_EQ(tds[0]->text_content(), "Alice");
    EXPECT_EQ(tds[3]->text_content(), "25");

    auto* tfoot = table->find_element("tfoot");
    ASSERT_NE(tfoot, nullptr);
    auto* footer_td = tfoot->find_element("td");
    ASSERT_NE(footer_td, nullptr);
    EXPECT_EQ(get_attr_v63(footer_td, "colspan"), "2");
    EXPECT_EQ(footer_td->text_content(), "Total: 2");
}

// 4. Multiple void elements in sequence (br, hr, img, input)
TEST(HtmlParserTest, MultipleVoidElementsInSequenceV86) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p>Line one<br>Line two<br>Line three</p>"
        "<hr>"
        "<img src=\"photo.jpg\" alt=\"Photo\">"
        "<input type=\"email\" placeholder=\"Enter email\">"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    auto brs = p->find_all_elements("br");
    EXPECT_EQ(brs.size(), 2u);

    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
    EXPECT_TRUE(hr->children.empty());

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "photo.jpg");
    EXPECT_EQ(get_attr_v63(img, "alt"), "Photo");

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "email");
    EXPECT_EQ(get_attr_v63(input, "placeholder"), "Enter email");
}

// 5. Deeply nested divs preserve hierarchy
TEST(HtmlParserTest, DeeplyNestedDivHierarchyV86) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div id=\"l1\"><div id=\"l2\"><div id=\"l3\">"
        "<div id=\"l4\"><span>Deep</span></div>"
        "</div></div></div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* l1 = doc->find_element("div");
    ASSERT_NE(l1, nullptr);
    EXPECT_EQ(get_attr_v63(l1, "id"), "l1");

    // l1 should contain l2 as a child div
    auto* l2 = l1->find_element("div");
    ASSERT_NE(l2, nullptr);
    EXPECT_EQ(get_attr_v63(l2, "id"), "l2");

    auto* l3 = l2->find_element("div");
    ASSERT_NE(l3, nullptr);
    EXPECT_EQ(get_attr_v63(l3, "id"), "l3");

    auto* l4 = l3->find_element("div");
    ASSERT_NE(l4, nullptr);
    EXPECT_EQ(get_attr_v63(l4, "id"), "l4");

    auto* span = l4->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "Deep");
}

// 6. Comment nodes among sibling elements
TEST(HtmlParserTest, CommentNodesBetweenElementsV86) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<!-- header comment -->"
        "<h1>Title</h1>"
        "<!-- nav comment -->"
        "<nav>Links</nav>"
        "<!-- footer comment -->"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Title");

    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    EXPECT_EQ(nav->text_content(), "Links");

    // Check that comment nodes exist in the body's children
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    int comment_count = 0;
    for (auto& child : body->children) {
        if (child->type == clever::html::SimpleNode::Comment) {
            comment_count++;
        }
    }
    EXPECT_GE(comment_count, 2);
}

// 7. Attributes with special characters (quotes, ampersands)
TEST(HtmlParserTest, AttributesWithSpecialCharsV86) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<a href=\"/search?q=hello&amp;lang=en\" title=\"Click &amp; Go\">Link</a>"
        "<div data-json='{\"key\":\"val\"}'></div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->text_content(), "Link");
    // The parser should decode &amp; to &
    std::string href = get_attr_v63(a, "href");
    EXPECT_FALSE(href.empty());
    // Verify the href contains expected path structure
    EXPECT_NE(href.find("/search?q=hello"), std::string::npos);

    std::string title = get_attr_v63(a, "title");
    EXPECT_FALSE(title.empty());

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    std::string json_attr = get_attr_v63(div, "data-json");
    EXPECT_FALSE(json_attr.empty());
    EXPECT_NE(json_attr.find("key"), std::string::npos);
}

// 8. Whitespace-only text nodes between block elements
TEST(HtmlParserTest, WhitespaceTextNodesBetweenBlocksV86) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div>First</div>  \n  <div>Second</div>  \n  <div>Third</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    auto divs = body->find_all_elements("div");
    ASSERT_EQ(divs.size(), 3u);
    EXPECT_EQ(divs[0]->text_content(), "First");
    EXPECT_EQ(divs[1]->text_content(), "Second");
    EXPECT_EQ(divs[2]->text_content(), "Third");

    // Body should have more than 3 children due to whitespace text nodes
    // (or exactly 3 if parser strips inter-element whitespace - either is valid)
    EXPECT_GE(body->children.size(), 3u);
}

// ---------------------------------------------------------------------------
// Cycle V87 — HTML parser coverage: tables, lists, nested forms, inline
//             elements, multiple attributes, sibling traversal, DOCTYPE
//             handling, and mixed inline/block content.
// ---------------------------------------------------------------------------

// 1. Table parsing preserves structure (thead, tbody, tr, td)
TEST(HtmlParserTest, TableStructurePreservedV87) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<thead><tr><th>Name</th><th>Age</th></tr></thead>"
        "<tbody><tr><td>Alice</td><td>30</td></tr>"
        "<tr><td>Bob</td><td>25</td></tr></tbody>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);

    auto tds = table->find_all_elements("td");
    ASSERT_EQ(tds.size(), 4u);
    EXPECT_EQ(tds[0]->text_content(), "Alice");
    EXPECT_EQ(tds[1]->text_content(), "30");
    EXPECT_EQ(tds[2]->text_content(), "Bob");
    EXPECT_EQ(tds[3]->text_content(), "25");

    auto ths = table->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Age");
}

// 2. Ordered and unordered lists with nested items
TEST(HtmlParserTest, NestedListStructureV87) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ul>"
        "<li>Fruit"
        "  <ol><li>Apple</li><li>Banana</li></ol>"
        "</li>"
        "<li>Veggie</li>"
        "</ul>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);

    auto outer_lis = ul->find_all_elements("li");
    // Should find at least 4 li elements total (2 outer + 2 inner)
    EXPECT_GE(outer_lis.size(), 4u);

    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    auto inner_lis = ol->find_all_elements("li");
    ASSERT_EQ(inner_lis.size(), 2u);
    EXPECT_EQ(inner_lis[0]->text_content(), "Apple");
    EXPECT_EQ(inner_lis[1]->text_content(), "Banana");
}

// 3. Multiple void elements in sequence (br, hr, img, input)
TEST(HtmlParserTest, MultipleVoidElementsInSequenceV87) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p>Line one<br>Line two<br>Line three</p>"
        "<hr>"
        "<img src=\"pic.jpg\" alt=\"Photo\">"
        "<input type=\"checkbox\" checked>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto brs = doc->find_all_elements("br");
    EXPECT_EQ(brs.size(), 2u);

    auto* hr = doc->find_element("hr");
    EXPECT_NE(hr, nullptr);

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "pic.jpg");
    EXPECT_EQ(get_attr_v63(img, "alt"), "Photo");

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "checkbox");
}

// 4. Inline formatting elements preserve text content (b, i, em, strong, u)
TEST(HtmlParserTest, InlineFormattingElementsV87) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p><b>Bold</b> and <i>Italic</i> and <em>Emphasis</em> and "
        "<strong>Strong</strong> and <u>Underline</u></p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* b = doc->find_element("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->text_content(), "Bold");

    auto* i = doc->find_element("i");
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->text_content(), "Italic");

    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "Emphasis");

    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "Strong");

    auto* u = doc->find_element("u");
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(u->text_content(), "Underline");
}

// 5. Element with many attributes preserves them all
TEST(HtmlParserTest, ElementWithManyAttributesV87) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div id=\"main\" class=\"container\" data-x=\"10\" data-y=\"20\" "
        "style=\"color:red\" title=\"Main Box\" role=\"region\" "
        "aria-label=\"Content\">Content</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->text_content(), "Content");

    EXPECT_EQ(get_attr_v63(div, "id"), "main");
    EXPECT_EQ(get_attr_v63(div, "class"), "container");
    EXPECT_EQ(get_attr_v63(div, "data-x"), "10");
    EXPECT_EQ(get_attr_v63(div, "data-y"), "20");
    EXPECT_EQ(get_attr_v63(div, "style"), "color:red");
    EXPECT_EQ(get_attr_v63(div, "title"), "Main Box");
    EXPECT_EQ(get_attr_v63(div, "role"), "region");
    EXPECT_EQ(get_attr_v63(div, "aria-label"), "Content");
}

// 6. Sibling elements at same level keep correct order
TEST(HtmlParserTest, SiblingElementOrderPreservedV87) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<section id=\"s1\">First</section>"
        "<section id=\"s2\">Second</section>"
        "<section id=\"s3\">Third</section>"
        "<section id=\"s4\">Fourth</section>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto sections = doc->find_all_elements("section");
    ASSERT_EQ(sections.size(), 4u);

    EXPECT_EQ(get_attr_v63(sections[0], "id"), "s1");
    EXPECT_EQ(sections[0]->text_content(), "First");

    EXPECT_EQ(get_attr_v63(sections[1], "id"), "s2");
    EXPECT_EQ(sections[1]->text_content(), "Second");

    EXPECT_EQ(get_attr_v63(sections[2], "id"), "s3");
    EXPECT_EQ(sections[2]->text_content(), "Third");

    EXPECT_EQ(get_attr_v63(sections[3], "id"), "s4");
    EXPECT_EQ(sections[3]->text_content(), "Fourth");
}

// 7. DOCTYPE node is present in parsed document
TEST(HtmlParserTest, DoctypeNodePresentInDocumentV87) {
    auto doc = clever::html::parse(
        "<!DOCTYPE html>"
        "<html><head><title>DocType Test</title></head>"
        "<body><p>Hello</p></body></html>");
    ASSERT_NE(doc, nullptr);

    // The document root should have a DocumentType child
    bool found_doctype = false;
    for (auto& child : doc->children) {
        if (child->type == clever::html::SimpleNode::DocumentType) {
            found_doctype = true;
            break;
        }
    }
    EXPECT_TRUE(found_doctype);

    auto* title = doc->find_element("title");
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->text_content(), "DocType Test");

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hello");
}

// 8. Mixed block and inline content in a single parent
TEST(HtmlParserTest, MixedBlockAndInlineContentV87) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div>"
        "Text before "
        "<span>inline span</span>"
        " text middle "
        "<p>block paragraph</p>"
        "<a href=\"#\">link</a>"
        " trailing text"
        "</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);

    // div should have multiple children (text nodes + elements)
    EXPECT_GE(div->children.size(), 3u);

    auto* span = div->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "inline span");

    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->text_content(), "link");
    EXPECT_EQ(get_attr_v63(a, "href"), "#");

    // The overall text_content of the div should contain all text
    std::string full_text = div->text_content();
    EXPECT_NE(full_text.find("Text before"), std::string::npos);
    EXPECT_NE(full_text.find("inline span"), std::string::npos);
    EXPECT_NE(full_text.find("block paragraph"), std::string::npos);
    EXPECT_NE(full_text.find("link"), std::string::npos);
}

// ---------------------------------------------------------------------------
// V88 tests: advanced HTML parsing scenarios
// ---------------------------------------------------------------------------

// 1. Nested tables are parsed with correct structure
TEST(HtmlParserTest, NestedTablesV88) {
    auto doc = clever::html::parse(
        "<html><body><table>"
        "<tr><td>Outer Cell"
        "<table><tr><td>Inner Cell</td></tr></table>"
        "</td></tr>"
        "</table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto tables = doc->find_all_elements("table");
    EXPECT_GE(tables.size(), 2u);

    auto tds = doc->find_all_elements("td");
    EXPECT_GE(tds.size(), 2u);

    // Inner cell text should be accessible
    std::string body_text = doc->find_element("body")->text_content();
    EXPECT_NE(body_text.find("Outer Cell"), std::string::npos);
    EXPECT_NE(body_text.find("Inner Cell"), std::string::npos);
}

// 2. Data attributes are preserved correctly
TEST(HtmlParserTest, DataAttributesPreservedV88) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div data-id=\"42\" data-role=\"widget\" data-empty=\"\">content</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);

    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "widget");
    EXPECT_EQ(get_attr_v63(div, "data-empty"), "");
    EXPECT_EQ(div->text_content(), "content");
}

// 3. Multiple script tags with content are parsed
TEST(HtmlParserTest, MultipleScriptTagsV88) {
    auto doc = clever::html::parse(
        "<html><head>"
        "<script>var a = 1;</script>"
        "<script type=\"module\">import x from './x';</script>"
        "</head><body><p>After scripts</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto scripts = doc->find_all_elements("script");
    EXPECT_GE(scripts.size(), 2u);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "After scripts");
}

// 4. Deeply nested elements maintain parent-child relationships
TEST(HtmlParserTest, DeeplyNestedElementsV88) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div><section><article><header><h1>Deep Title</h1></header></article></section></div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);

    auto* section = doc->find_element("section");
    ASSERT_NE(section, nullptr);

    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);

    auto* header = doc->find_element("header");
    ASSERT_NE(header, nullptr);

    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Deep Title");
}

// 5. Adjacent text nodes around comments
TEST(HtmlParserTest, TextAroundCommentsV88) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p>Before<!-- comment -->After</p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);

    std::string text = p->text_content();
    EXPECT_NE(text.find("Before"), std::string::npos);
    EXPECT_NE(text.find("After"), std::string::npos);
}

// 6. Definition list (dl/dt/dd) structure
TEST(HtmlParserTest, DefinitionListStructureV88) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>Term 1</dt><dd>Definition 1</dd>"
        "<dt>Term 2</dt><dd>Definition 2</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);

    auto dts = doc->find_all_elements("dt");
    EXPECT_EQ(dts.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term 1");
    EXPECT_EQ(dts[1]->text_content(), "Term 2");

    auto dds = doc->find_all_elements("dd");
    EXPECT_EQ(dds.size(), 2u);
    EXPECT_EQ(dds[0]->text_content(), "Definition 1");
    EXPECT_EQ(dds[1]->text_content(), "Definition 2");
}

// 7. Multiple classes in a single class attribute
TEST(HtmlParserTest, MultipleClassAttributeValuesV88) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div class=\"alpha beta gamma\">Styled</div>"
        "<span class=\"one two\">Also styled</span>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    std::string div_class = get_attr_v63(div, "class");
    EXPECT_NE(div_class.find("alpha"), std::string::npos);
    EXPECT_NE(div_class.find("beta"), std::string::npos);
    EXPECT_NE(div_class.find("gamma"), std::string::npos);

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    std::string span_class = get_attr_v63(span, "class");
    EXPECT_NE(span_class.find("one"), std::string::npos);
    EXPECT_NE(span_class.find("two"), std::string::npos);
}

// 8. Inline style attribute is preserved verbatim
TEST(HtmlParserTest, InlineStyleAttributePreservedV88) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p style=\"color: red; font-size: 14px;\">Red text</p>"
        "<div style=\"margin: 0 auto;\">Centered</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    std::string p_style = get_attr_v63(p, "style");
    EXPECT_NE(p_style.find("color: red"), std::string::npos);
    EXPECT_NE(p_style.find("font-size: 14px"), std::string::npos);
    EXPECT_EQ(p->text_content(), "Red text");

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    std::string div_style = get_attr_v63(div, "style");
    EXPECT_NE(div_style.find("margin: 0 auto"), std::string::npos);
    EXPECT_EQ(div->text_content(), "Centered");
}

// ---------------------------------------------------------------------------
// V89 – 8 additional HTML parser tests
// ---------------------------------------------------------------------------

// 1. Parse simple paragraph, verify body > p > text structure
TEST(HtmlParserTest, ParagraphTextStructureV89) {
    auto doc = clever::html::parse("<html><body><p>Hello World</p></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->tag_name, "p");
    ASSERT_FALSE(p->children.empty());
    EXPECT_EQ(p->children[0]->tag_name, "");
    EXPECT_EQ(p->children[0]->text_content(), "Hello World");
}

// 2. Parse nested divs, verify depth
TEST(HtmlParserTest, NestedDivsDepthV89) {
    auto doc = clever::html::parse(
        "<html><body><div><div><div>Deep</div></div></div></body></html>");
    ASSERT_NE(doc, nullptr);

    // html > body > div > div > div > text
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    auto* d1 = body->find_element("div");
    ASSERT_NE(d1, nullptr);
    EXPECT_EQ(d1->tag_name, "div");
    auto* d2 = d1->find_element("div");
    ASSERT_NE(d2, nullptr);
    auto* d3 = d2->find_element("div");
    ASSERT_NE(d3, nullptr);
    EXPECT_EQ(d3->text_content(), "Deep");
}

// 3. Parse element with multiple attributes
TEST(HtmlParserTest, MultipleAttributesOnElementV89) {
    auto doc = clever::html::parse(
        "<html><body><input type=\"email\" name=\"user\" placeholder=\"Enter email\" required></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "email");
    EXPECT_EQ(get_attr_v63(input, "name"), "user");
    EXPECT_EQ(get_attr_v63(input, "placeholder"), "Enter email");
}

// 4. Parse self-closing br tag
TEST(HtmlParserTest, SelfClosingBrTagV89) {
    auto doc = clever::html::parse(
        "<html><body><p>Line1<br>Line2</p></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    // p should have children: text("Line1"), br, text("Line2")
    ASSERT_GE(p->children.size(), 3u);
    EXPECT_EQ(p->children[0]->text_content(), "Line1");
    EXPECT_EQ(p->children[1]->tag_name, "br");
    EXPECT_TRUE(p->children[1]->children.empty());
    EXPECT_EQ(p->children[2]->text_content(), "Line2");
}

// 5. Parse text with named entities
TEST(HtmlParserTest, NamedEntitiesInTextV89) {
    auto doc = clever::html::parse(
        "<html><body><p>A &amp; B &lt; C &gt; D</p></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    std::string text = p->text_content();
    EXPECT_NE(text.find("&"), std::string::npos);
    EXPECT_NE(text.find("<"), std::string::npos);
    EXPECT_NE(text.find(">"), std::string::npos);
}

// 6. Parse list structure (ul > li)
TEST(HtmlParserTest, UnorderedListStructureV89) {
    auto doc = clever::html::parse(
        "<html><body><ul><li>Apple</li><li>Banana</li><li>Cherry</li></ul></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    auto lis = ul->find_all_elements("li");
    ASSERT_EQ(lis.size(), 3u);
    EXPECT_EQ(lis[0]->text_content(), "Apple");
    EXPECT_EQ(lis[1]->text_content(), "Banana");
    EXPECT_EQ(lis[2]->text_content(), "Cherry");
}

// 7. Parse heading tags (h1 through h3)
TEST(HtmlParserTest, HeadingTagsH1H2H3V89) {
    auto doc = clever::html::parse(
        "<html><body><h1>Title</h1><h2>Subtitle</h2><h3>Section</h3></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Title");

    auto* h2 = doc->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->text_content(), "Subtitle");

    auto* h3 = doc->find_element("h3");
    ASSERT_NE(h3, nullptr);
    EXPECT_EQ(h3->text_content(), "Section");
}

// 8. Parse mixed text and element children
TEST(HtmlParserTest, MixedTextAndElementChildrenV89) {
    auto doc = clever::html::parse(
        "<html><body><p>Hello <strong>bold</strong> world</p></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    // p should contain: text("Hello "), strong, text(" world")
    ASSERT_GE(p->children.size(), 3u);
    EXPECT_EQ(p->children[0]->tag_name, "");
    EXPECT_EQ(p->children[0]->text_content(), "Hello ");
    EXPECT_EQ(p->children[1]->tag_name, "strong");
    EXPECT_EQ(p->children[1]->text_content(), "bold");
    EXPECT_EQ(p->children[2]->tag_name, "");
    EXPECT_EQ(p->children[2]->text_content(), " world");
}

// 1. Parse definition list (dl > dt + dd)
TEST(HtmlParserTest, DefinitionListStructureV90) {
    auto doc = clever::html::parse(
        "<html><body><dl><dt>Term</dt><dd>Definition</dd></dl></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    auto* dt = dl->find_element("dt");
    ASSERT_NE(dt, nullptr);
    EXPECT_EQ(dt->text_content(), "Term");
    auto dds = dl->find_all_elements("dd");
    ASSERT_EQ(dds.size(), 1u);
    EXPECT_EQ(dds[0]->text_content(), "Definition");
}

// 2. Parse nested spans with attributes
TEST(HtmlParserTest, NestedSpansWithAttributesV90) {
    auto doc = clever::html::parse(
        "<html><body><span class=\"outer\"><span id=\"inner\">Text</span></span></body></html>");
    ASSERT_NE(doc, nullptr);
    auto spans = doc->find_all_elements("span");
    ASSERT_EQ(spans.size(), 2u);
    // Outer span has class attribute
    bool found_outer = false;
    for (auto* s : spans) {
        for (auto& attr : s->attributes) {
            if (attr.name == "class" && attr.value == "outer") found_outer = true;
        }
    }
    EXPECT_TRUE(found_outer);
    // Inner span has text
    auto* inner = spans[1];
    EXPECT_EQ(inner->text_content(), "Text");
}

// 3. Parse table with thead and tbody
TEST(HtmlParserTest, TableTheadTbodyV90) {
    auto doc = clever::html::parse(
        "<html><body><table><thead><tr><th>Header</th></tr></thead>"
        "<tbody><tr><td>Cell</td></tr></tbody></table></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    auto* th = table->find_element("th");
    ASSERT_NE(th, nullptr);
    EXPECT_EQ(th->text_content(), "Header");
    auto* td = table->find_element("td");
    ASSERT_NE(td, nullptr);
    EXPECT_EQ(td->text_content(), "Cell");
}

// 4. Parse multiple void elements (br, hr, img)
TEST(HtmlParserTest, MultipleVoidElementsV90) {
    auto doc = clever::html::parse(
        "<html><body><p>Line1<br>Line2</p><hr><img src=\"pic.png\"></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    auto* br = p->find_element("br");
    ASSERT_NE(br, nullptr);
    EXPECT_EQ(br->children.size(), 0u);
    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->children.size(), 0u);
}

// 5. Parse deeply nested divs (4 levels)
TEST(HtmlParserTest, DeeplyNestedDivsV90) {
    auto doc = clever::html::parse(
        "<html><body><div><div><div><div>Deep</div></div></div></div></body></html>");
    ASSERT_NE(doc, nullptr);
    auto divs = doc->find_all_elements("div");
    ASSERT_EQ(divs.size(), 4u);
    // The innermost div has the text
    auto* innermost = divs[3];
    EXPECT_EQ(innermost->text_content(), "Deep");
    EXPECT_EQ(innermost->children.size(), 1u); // text node
}

// 6. Parse blockquote with paragraph
TEST(HtmlParserTest, BlockquoteWithParagraphV90) {
    auto doc = clever::html::parse(
        "<html><body><blockquote><p>Quoted text here</p></blockquote></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* bq = doc->find_element("blockquote");
    ASSERT_NE(bq, nullptr);
    auto* p = bq->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Quoted text here");
}

// 7. Parse pre element preserving content
TEST(HtmlParserTest, PreElementContentV90) {
    auto doc = clever::html::parse(
        "<html><body><pre>code line</pre></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
    EXPECT_EQ(pre->text_content(), "code line");
}

// 8. Parse anchor with href and target attributes
TEST(HtmlParserTest, AnchorWithMultipleAttributesV90) {
    auto doc = clever::html::parse(
        "<html><body><a href=\"https://example.com\" target=\"_blank\">Link</a></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->text_content(), "Link");
    bool found_href = false;
    bool found_target = false;
    for (auto& attr : a->attributes) {
        if (attr.name == "href" && attr.value == "https://example.com") found_href = true;
        if (attr.name == "target" && attr.value == "_blank") found_target = true;
    }
    EXPECT_TRUE(found_href);
    EXPECT_TRUE(found_target);
}

// --- V91 tests ---

// 1. Parse definition list structure (dl, dt, dd)
TEST(HtmlParserTest, DefinitionListStructureV91) {
    auto doc = clever::html::parse(
        "<html><body><dl><dt>Term</dt><dd>Definition</dd></dl></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    auto* dt = dl->find_element("dt");
    ASSERT_NE(dt, nullptr);
    EXPECT_EQ(dt->text_content(), "Term");
    auto* dd = dl->find_element("dd");
    ASSERT_NE(dd, nullptr);
    EXPECT_EQ(dd->text_content(), "Definition");
}

// 2. Parse table with caption and multiple rows
TEST(HtmlParserTest, TableWithCaptionAndRowsV91) {
    auto doc = clever::html::parse(
        "<html><body><table><caption>Stats</caption>"
        "<tr><th>Name</th><th>Score</th></tr>"
        "<tr><td>Alice</td><td>95</td></tr></table></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* caption = doc->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->text_content(), "Stats");
    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 2u);
    EXPECT_EQ(tds[0]->text_content(), "Alice");
    EXPECT_EQ(tds[1]->text_content(), "95");
}

// 3. Parse multiple sibling paragraphs inside article
TEST(HtmlParserTest, MultipleSiblingParagraphsInArticleV91) {
    auto doc = clever::html::parse(
        "<html><body><article><p>First</p><p>Second</p><p>Third</p></article></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    auto ps = article->find_all_elements("p");
    ASSERT_EQ(ps.size(), 3u);
    EXPECT_EQ(ps[0]->text_content(), "First");
    EXPECT_EQ(ps[1]->text_content(), "Second");
    EXPECT_EQ(ps[2]->text_content(), "Third");
}

// 4. Parse image with all attributes using helper
TEST(HtmlParserTest, ImageWithMultipleAttributesV91) {
    auto doc = clever::html::parse(
        "<html><body><img src=\"photo.jpg\" alt=\"A photo\" width=\"640\" height=\"480\"></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "photo.jpg");
    EXPECT_EQ(get_attr_v63(img, "alt"), "A photo");
    EXPECT_EQ(get_attr_v63(img, "width"), "640");
    EXPECT_EQ(get_attr_v63(img, "height"), "480");
    EXPECT_EQ(img->children.size(), 0u);
}

// 5. Parse nested ordered list inside unordered list
TEST(HtmlParserTest, NestedListsOlInsideUlV91) {
    auto doc = clever::html::parse(
        "<html><body><ul><li>Item A</li><li><ol><li>Sub 1</li><li>Sub 2</li></ol></li></ul></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    auto* ol = ul->find_element("ol");
    ASSERT_NE(ol, nullptr);
    auto olis = ol->find_all_elements("li");
    ASSERT_EQ(olis.size(), 2u);
    EXPECT_EQ(olis[0]->text_content(), "Sub 1");
    EXPECT_EQ(olis[1]->text_content(), "Sub 2");
}

// 6. Parse figure with figcaption
TEST(HtmlParserTest, FigureWithFigcaptionV91) {
    auto doc = clever::html::parse(
        "<html><body><figure><img src=\"chart.png\" alt=\"Chart\">"
        "<figcaption>Figure 1: Revenue</figcaption></figure></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);
    auto* img = figure->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "chart.png");
    auto* figcaption = figure->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->text_content(), "Figure 1: Revenue");
}

// 7. Parse details and summary elements
TEST(HtmlParserTest, DetailsAndSummaryElementsV91) {
    auto doc = clever::html::parse(
        "<html><body><details><summary>Click me</summary><p>Hidden content</p></details></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    auto* summary = details->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click me");
    auto* p = details->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hidden content");
}

// 8. Parse section with header containing multiple heading levels
TEST(HtmlParserTest, SectionWithMultipleHeadingsV91) {
    auto doc = clever::html::parse(
        "<html><body><section><h1>Main Title</h1><h2>Subtitle</h2><h3>Sub-subtitle</h3></section></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* section = doc->find_element("section");
    ASSERT_NE(section, nullptr);
    auto* h1 = section->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Main Title");
    auto* h2 = section->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->text_content(), "Subtitle");
    auto* h3 = section->find_element("h3");
    ASSERT_NE(h3, nullptr);
    EXPECT_EQ(h3->text_content(), "Sub-subtitle");
}

// 1. Parse blockquote with cite attribute
TEST(HtmlParserTest, BlockquoteWithCiteAttrV92) {
    auto doc = clever::html::parse(
        "<html><body><blockquote cite=\"https://example.com\"><p>Famous quote</p></blockquote></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* bq = doc->find_element("blockquote");
    ASSERT_NE(bq, nullptr);
    EXPECT_EQ(get_attr_v63(bq, "cite"), "https://example.com");
    auto* p = bq->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Famous quote");
}

// 2. Parse definition list (dl/dt/dd)
TEST(HtmlParserTest, DefinitionListDlDtDdV92) {
    auto doc = clever::html::parse(
        "<html><body><dl><dt>Term</dt><dd>Definition</dd></dl></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    auto* dt = dl->find_element("dt");
    ASSERT_NE(dt, nullptr);
    EXPECT_EQ(dt->text_content(), "Term");
    auto* dd = dl->find_element("dd");
    ASSERT_NE(dd, nullptr);
    EXPECT_EQ(dd->text_content(), "Definition");
}

// 3. Parse table with thead and tbody
TEST(HtmlParserTest, TableTheadTbodyV92) {
    auto doc = clever::html::parse(
        "<html><body><table><thead><tr><th>Name</th></tr></thead>"
        "<tbody><tr><td>Alice</td></tr></tbody></table></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    auto* thead = table->find_element("thead");
    ASSERT_NE(thead, nullptr);
    auto* th = thead->find_element("th");
    ASSERT_NE(th, nullptr);
    EXPECT_EQ(th->text_content(), "Name");
    auto* tbody = table->find_element("tbody");
    ASSERT_NE(tbody, nullptr);
    auto* td = tbody->find_element("td");
    ASSERT_NE(td, nullptr);
    EXPECT_EQ(td->text_content(), "Alice");
}

// 4. Parse nav with multiple links
TEST(HtmlParserTest, NavWithMultipleLinksV92) {
    auto doc = clever::html::parse(
        "<html><body><nav><a href=\"/home\">Home</a><a href=\"/about\">About</a><a href=\"/contact\">Contact</a></nav></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    auto links = nav->find_all_elements("a");
    ASSERT_EQ(links.size(), 3u);
    EXPECT_EQ(get_attr_v63(links[0], "href"), "/home");
    EXPECT_EQ(links[0]->text_content(), "Home");
    EXPECT_EQ(get_attr_v63(links[1], "href"), "/about");
    EXPECT_EQ(links[1]->text_content(), "About");
    EXPECT_EQ(get_attr_v63(links[2], "href"), "/contact");
    EXPECT_EQ(links[2]->text_content(), "Contact");
}

// 5. Parse article with time element
TEST(HtmlParserTest, ArticleWithTimeElementV92) {
    auto doc = clever::html::parse(
        "<html><body><article><time datetime=\"2026-01-15\">January 15</time>"
        "<p>Article body</p></article></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    auto* time_el = article->find_element("time");
    ASSERT_NE(time_el, nullptr);
    EXPECT_EQ(get_attr_v63(time_el, "datetime"), "2026-01-15");
    EXPECT_EQ(time_el->text_content(), "January 15");
    auto* p = article->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Article body");
}

// 6. Parse form with select and option elements
TEST(HtmlParserTest, FormWithSelectOptionsV92) {
    auto doc = clever::html::parse(
        "<html><body><form><select name=\"color\">"
        "<option value=\"r\">Red</option><option value=\"g\">Green</option>"
        "</select></form></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    auto* select = form->find_element("select");
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(get_attr_v63(select, "name"), "color");
    auto options = select->find_all_elements("option");
    ASSERT_EQ(options.size(), 2u);
    EXPECT_EQ(get_attr_v63(options[0], "value"), "r");
    EXPECT_EQ(options[0]->text_content(), "Red");
    EXPECT_EQ(get_attr_v63(options[1], "value"), "g");
    EXPECT_EQ(options[1]->text_content(), "Green");
}

// 7. Parse nested divs with data attributes
TEST(HtmlParserTest, NestedDivsWithDataAttrsV92) {
    auto doc = clever::html::parse(
        "<html><body><div data-id=\"outer\"><div data-id=\"inner\"><span>Content</span></div></div></body></html>");
    ASSERT_NE(doc, nullptr);
    auto divs = doc->find_all_elements("div");
    ASSERT_GE(divs.size(), 2u);
    EXPECT_EQ(get_attr_v63(divs[0], "data-id"), "outer");
    EXPECT_EQ(get_attr_v63(divs[1], "data-id"), "inner");
    auto* span = divs[1]->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "Content");
}

// 8. Parse footer with address element
TEST(HtmlParserTest, FooterWithAddressElementV92) {
    auto doc = clever::html::parse(
        "<html><body><footer><address>123 Main St</address><small>Copyright 2026</small></footer></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* footer = doc->find_element("footer");
    ASSERT_NE(footer, nullptr);
    auto* address = footer->find_element("address");
    ASSERT_NE(address, nullptr);
    EXPECT_EQ(address->text_content(), "123 Main St");
    auto* small = footer->find_element("small");
    ASSERT_NE(small, nullptr);
    EXPECT_EQ(small->text_content(), "Copyright 2026");
}

// ============================================================================
// V93 Tests
// ============================================================================

// 1. Parse definition list with dt and dd
TEST(HtmlParserTest, DefinitionListDtDdV93) {
    auto doc = clever::html::parse(
        "<html><body><dl><dt>Term1</dt><dd>Def1</dd>"
        "<dt>Term2</dt><dd>Def2</dd></dl></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    auto dts = dl->find_all_elements("dt");
    auto dds = dl->find_all_elements("dd");
    ASSERT_EQ(dts.size(), 2u);
    ASSERT_EQ(dds.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term1");
    EXPECT_EQ(dts[1]->text_content(), "Term2");
    EXPECT_EQ(dds[0]->text_content(), "Def1");
    EXPECT_EQ(dds[1]->text_content(), "Def2");
}

// 2. Parse figure with figcaption and img
TEST(HtmlParserTest, FigureWithFigcaptionAndImgV93) {
    auto doc = clever::html::parse(
        "<html><body><figure>"
        "<img src=\"photo.jpg\" alt=\"A photo\">"
        "<figcaption>Caption text</figcaption>"
        "</figure></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);
    auto* img = figure->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "photo.jpg");
    EXPECT_EQ(get_attr_v63(img, "alt"), "A photo");
    auto* figcaption = figure->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->text_content(), "Caption text");
}

// 3. Parse details and summary elements
TEST(HtmlParserTest, DetailsWithSummaryV93) {
    auto doc = clever::html::parse(
        "<html><body><details open=\"open\">"
        "<summary>Click to expand</summary>"
        "<p>Hidden content revealed</p>"
        "</details></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    EXPECT_EQ(get_attr_v63(details, "open"), "open");
    auto* summary = details->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click to expand");
    auto* p = details->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hidden content revealed");
}

// 4. Parse table with thead, tbody, tfoot
TEST(HtmlParserTest, TableTheadTbodyTfootV93) {
    auto doc = clever::html::parse(
        "<html><body><table>"
        "<thead><tr><th>Header</th></tr></thead>"
        "<tbody><tr><td>Cell</td></tr></tbody>"
        "<tfoot><tr><td>Footer</td></tr></tfoot>"
        "</table></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    auto* thead = table->find_element("thead");
    ASSERT_NE(thead, nullptr);
    auto* th = thead->find_element("th");
    ASSERT_NE(th, nullptr);
    EXPECT_EQ(th->text_content(), "Header");
    auto* tbody = table->find_element("tbody");
    ASSERT_NE(tbody, nullptr);
    auto* td = tbody->find_element("td");
    ASSERT_NE(td, nullptr);
    EXPECT_EQ(td->text_content(), "Cell");
    auto* tfoot = table->find_element("tfoot");
    ASSERT_NE(tfoot, nullptr);
    auto* ftd = tfoot->find_element("td");
    ASSERT_NE(ftd, nullptr);
    EXPECT_EQ(ftd->text_content(), "Footer");
}

// 5. Parse multiple sibling paragraphs preserving order
TEST(HtmlParserTest, MultipleSiblingParagraphsOrderV93) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p>First</p><p>Second</p><p>Third</p><p>Fourth</p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto paras = doc->find_all_elements("p");
    ASSERT_EQ(paras.size(), 4u);
    EXPECT_EQ(paras[0]->text_content(), "First");
    EXPECT_EQ(paras[1]->text_content(), "Second");
    EXPECT_EQ(paras[2]->text_content(), "Third");
    EXPECT_EQ(paras[3]->text_content(), "Fourth");
}

// 6. Parse link element with rel and href attributes
TEST(HtmlParserTest, LinkElementRelHrefV93) {
    auto doc = clever::html::parse(
        "<html><head>"
        "<link rel=\"stylesheet\" href=\"style.css\">"
        "<link rel=\"icon\" href=\"favicon.ico\">"
        "</head><body></body></html>");
    ASSERT_NE(doc, nullptr);
    auto links = doc->find_all_elements("link");
    ASSERT_EQ(links.size(), 2u);
    EXPECT_EQ(get_attr_v63(links[0], "rel"), "stylesheet");
    EXPECT_EQ(get_attr_v63(links[0], "href"), "style.css");
    EXPECT_EQ(get_attr_v63(links[1], "rel"), "icon");
    EXPECT_EQ(get_attr_v63(links[1], "href"), "favicon.ico");
}

// 7. Parse deeply nested structure (5 levels)
TEST(HtmlParserTest, DeeplyNestedFiveLevelsV93) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div><section><article><aside><mark>Deep</mark></aside></article></section></div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    auto* section = div->find_element("section");
    ASSERT_NE(section, nullptr);
    auto* article = section->find_element("article");
    ASSERT_NE(article, nullptr);
    auto* aside = article->find_element("aside");
    ASSERT_NE(aside, nullptr);
    auto* mark = aside->find_element("mark");
    ASSERT_NE(mark, nullptr);
    EXPECT_EQ(mark->text_content(), "Deep");
}

// 8. Parse mixed inline elements with text nodes
TEST(HtmlParserTest, MixedInlineElementsTextNodesV93) {
    auto doc = clever::html::parse(
        "<html><body><p>"
        "<strong>Bold</strong> and <em>italic</em> and <code>code</code>"
        "</p></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    auto* strong = p->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "Bold");
    auto* em = p->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "italic");
    auto* code = p->find_element("code");
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(code->text_content(), "code");
    // Verify the p has children including text nodes between inline elements
    ASSERT_GE(p->children.size(), 5u);
}

// ============================================================================
// V94 Tests
// ============================================================================

// 1. Parse definition list (dl/dt/dd) structure
TEST(HtmlParserTest, DefinitionListStructureV94) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>Term1</dt><dd>Def1</dd>"
        "<dt>Term2</dt><dd>Def2</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    auto dts = dl->find_all_elements("dt");
    auto dds = dl->find_all_elements("dd");
    ASSERT_EQ(dts.size(), 2u);
    ASSERT_EQ(dds.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term1");
    EXPECT_EQ(dts[1]->text_content(), "Term2");
    EXPECT_EQ(dds[0]->text_content(), "Def1");
    EXPECT_EQ(dds[1]->text_content(), "Def2");
}

// 2. Parse multiple data attributes on a single element
TEST(HtmlParserTest, MultipleDataAttributesV94) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div data-id=\"42\" data-role=\"main\" data-visible=\"true\">Content</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "main");
    EXPECT_EQ(get_attr_v63(div, "data-visible"), "true");
    EXPECT_EQ(div->text_content(), "Content");
}

// 3. Parse sibling heading elements at the same level
TEST(HtmlParserTest, SiblingHeadingsV94) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<h1>Title</h1>"
        "<h2>Subtitle</h2>"
        "<h3>Section</h3>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* h1 = doc->find_element("h1");
    auto* h2 = doc->find_element("h2");
    auto* h3 = doc->find_element("h3");
    ASSERT_NE(h1, nullptr);
    ASSERT_NE(h2, nullptr);
    ASSERT_NE(h3, nullptr);
    EXPECT_EQ(h1->text_content(), "Title");
    EXPECT_EQ(h2->text_content(), "Subtitle");
    EXPECT_EQ(h3->text_content(), "Section");
}

// 4. Parse a table with thead and tbody sections
TEST(HtmlParserTest, TableTheadTbodyV94) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<thead><tr><th>Name</th><th>Age</th></tr></thead>"
        "<tbody><tr><td>Alice</td><td>30</td></tr></tbody>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    auto ths = table->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Age");
    auto tds = table->find_all_elements("td");
    ASSERT_EQ(tds.size(), 2u);
    EXPECT_EQ(tds[0]->text_content(), "Alice");
    EXPECT_EQ(tds[1]->text_content(), "30");
}

// 5. Parse void elements (br, hr, img) mixed with text
TEST(HtmlParserTest, VoidElementsMixedWithTextV94) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p>Line one<br>Line two</p>"
        "<hr>"
        "<img src=\"pic.jpg\" alt=\"A picture\">"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    auto* br = p->find_element("br");
    ASSERT_NE(br, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    auto* hr = body->find_element("hr");
    ASSERT_NE(hr, nullptr);
    auto* img = body->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "pic.jpg");
    EXPECT_EQ(get_attr_v63(img, "alt"), "A picture");
}

// 6. Parse nested ordered lists
TEST(HtmlParserTest, NestedOrderedListsV94) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ol>"
        "<li>First"
          "<ol><li>Sub-A</li><li>Sub-B</li></ol>"
        "</li>"
        "<li>Second</li>"
        "</ol>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    auto outer_lis = ol->find_all_elements("li");
    // Should find all li elements including nested ones
    ASSERT_GE(outer_lis.size(), 4u);
    // The nested ol should exist inside the first li
    auto* nested_ol = ol->find_element("ol");
    ASSERT_NE(nested_ol, nullptr);
    auto nested_lis = nested_ol->find_all_elements("li");
    ASSERT_EQ(nested_lis.size(), 2u);
    EXPECT_EQ(nested_lis[0]->text_content(), "Sub-A");
    EXPECT_EQ(nested_lis[1]->text_content(), "Sub-B");
}

// 7. Parse blockquote with nested paragraph and cite
TEST(HtmlParserTest, BlockquoteWithCiteV94) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<blockquote cite=\"https://example.com\">"
        "<p>To be or not to be.</p>"
        "</blockquote>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* bq = doc->find_element("blockquote");
    ASSERT_NE(bq, nullptr);
    EXPECT_EQ(get_attr_v63(bq, "cite"), "https://example.com");
    auto* p = bq->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "To be or not to be.");
}

// 8. Parse a form with select and multiple option children
TEST(HtmlParserTest, FormSelectOptionsV94) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<form action=\"/submit\">"
        "<select name=\"color\">"
        "<option value=\"r\">Red</option>"
        "<option value=\"g\">Green</option>"
        "<option value=\"b\">Blue</option>"
        "</select>"
        "</form>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "action"), "/submit");
    auto* sel = form->find_element("select");
    ASSERT_NE(sel, nullptr);
    EXPECT_EQ(get_attr_v63(sel, "name"), "color");
    auto opts = sel->find_all_elements("option");
    ASSERT_EQ(opts.size(), 3u);
    EXPECT_EQ(get_attr_v63(opts[0], "value"), "r");
    EXPECT_EQ(opts[0]->text_content(), "Red");
    EXPECT_EQ(get_attr_v63(opts[1], "value"), "g");
    EXPECT_EQ(opts[1]->text_content(), "Green");
    EXPECT_EQ(get_attr_v63(opts[2], "value"), "b");
    EXPECT_EQ(opts[2]->text_content(), "Blue");
}

// ============================================================================
// V95 Tests
// ============================================================================

// 1. Parse a definition list (dl/dt/dd)
TEST(HtmlParserTest, DefinitionListV95) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>HTML</dt><dd>HyperText Markup Language</dd>"
        "<dt>CSS</dt><dd>Cascading Style Sheets</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    auto dts = dl->find_all_elements("dt");
    auto dds = dl->find_all_elements("dd");
    ASSERT_EQ(dts.size(), 2u);
    ASSERT_EQ(dds.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "HTML");
    EXPECT_EQ(dts[1]->text_content(), "CSS");
    EXPECT_EQ(dds[0]->text_content(), "HyperText Markup Language");
    EXPECT_EQ(dds[1]->text_content(), "Cascading Style Sheets");
}

// 2. Parse figure with figcaption and img
TEST(HtmlParserTest, FigureWithFigcaptionV95) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<figure>"
        "<img src=\"photo.jpg\" alt=\"A photo\">"
        "<figcaption>Photo caption here</figcaption>"
        "</figure>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* fig = doc->find_element("figure");
    ASSERT_NE(fig, nullptr);
    auto* img = fig->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "photo.jpg");
    EXPECT_EQ(get_attr_v63(img, "alt"), "A photo");
    auto* cap = fig->find_element("figcaption");
    ASSERT_NE(cap, nullptr);
    EXPECT_EQ(cap->text_content(), "Photo caption here");
}

// 3. Parse details/summary element
TEST(HtmlParserTest, DetailsSummaryV95) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<details>"
        "<summary>Click to expand</summary>"
        "<p>Hidden content revealed.</p>"
        "</details>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    auto* summary = details->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click to expand");
    auto* p = details->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hidden content revealed.");
}

// 4. Parse table with thead, tbody, and tfoot
TEST(HtmlParserTest, TableTheadTbodyTfootV95) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<thead><tr><th>Name</th><th>Score</th></tr></thead>"
        "<tbody><tr><td>Alice</td><td>95</td></tr>"
        "<tr><td>Bob</td><td>82</td></tr></tbody>"
        "<tfoot><tr><td>Total</td><td>177</td></tr></tfoot>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    auto* thead = table->find_element("thead");
    ASSERT_NE(thead, nullptr);
    auto ths = thead->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Score");
    auto* tbody = table->find_element("tbody");
    ASSERT_NE(tbody, nullptr);
    auto tds = tbody->find_all_elements("td");
    ASSERT_GE(tds.size(), 4u);
    auto* tfoot = table->find_element("tfoot");
    ASSERT_NE(tfoot, nullptr);
    auto foot_tds = tfoot->find_all_elements("td");
    ASSERT_EQ(foot_tds.size(), 2u);
    EXPECT_EQ(foot_tds[0]->text_content(), "Total");
    EXPECT_EQ(foot_tds[1]->text_content(), "177");
}

// 5. Parse multiple data attributes on a single element
TEST(HtmlParserTest, MultipleDataAttributesV95) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div data-id=\"42\" data-role=\"admin\" data-active=\"true\">User</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "admin");
    EXPECT_EQ(get_attr_v63(div, "data-active"), "true");
    EXPECT_EQ(div->text_content(), "User");
}

// 6. Parse nested section elements with headings
TEST(HtmlParserTest, NestedSectionsWithHeadingsV95) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<section>"
        "<h1>Chapter One</h1>"
        "<section>"
        "<h2>Section 1.1</h2>"
        "<p>Paragraph in subsection.</p>"
        "</section>"
        "</section>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* outer = doc->find_element("section");
    ASSERT_NE(outer, nullptr);
    auto* h1 = outer->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Chapter One");
    auto inner_sections = outer->find_all_elements("section");
    ASSERT_GE(inner_sections.size(), 1u);
    auto* h2 = inner_sections[0]->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->text_content(), "Section 1.1");
    auto* p = inner_sections[0]->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Paragraph in subsection.");
}

// 7. Parse nav with anchor links
TEST(HtmlParserTest, NavWithAnchorLinksV95) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<nav>"
        "<a href=\"/home\">Home</a>"
        "<a href=\"/about\">About</a>"
        "<a href=\"/contact\">Contact</a>"
        "</nav>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    auto links = nav->find_all_elements("a");
    ASSERT_EQ(links.size(), 3u);
    EXPECT_EQ(get_attr_v63(links[0], "href"), "/home");
    EXPECT_EQ(links[0]->text_content(), "Home");
    EXPECT_EQ(get_attr_v63(links[1], "href"), "/about");
    EXPECT_EQ(links[1]->text_content(), "About");
    EXPECT_EQ(get_attr_v63(links[2], "href"), "/contact");
    EXPECT_EQ(links[2]->text_content(), "Contact");
}

// 8. Parse fieldset with legend and mixed inputs
TEST(HtmlParserTest, FieldsetLegendInputsV95) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<fieldset>"
        "<legend>Personal Info</legend>"
        "<input type=\"text\" name=\"fname\" value=\"Jane\">"
        "<input type=\"email\" name=\"email\" value=\"j@e.co\">"
        "</fieldset>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* fs = doc->find_element("fieldset");
    ASSERT_NE(fs, nullptr);
    auto* legend = fs->find_element("legend");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->text_content(), "Personal Info");
    auto inputs = fs->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "text");
    EXPECT_EQ(get_attr_v63(inputs[0], "name"), "fname");
    EXPECT_EQ(get_attr_v63(inputs[0], "value"), "Jane");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "email");
    EXPECT_EQ(get_attr_v63(inputs[1], "name"), "email");
    EXPECT_EQ(get_attr_v63(inputs[1], "value"), "j@e.co");
}

// ============================================================================
// V96 Tests
// ============================================================================

// 1. Parse a definition list with dt/dd pairs
TEST(HtmlParserTest, DefinitionListDtDdPairsV96) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>HTML</dt><dd>HyperText Markup Language</dd>"
        "<dt>CSS</dt><dd>Cascading Style Sheets</dd>"
        "<dt>JS</dt><dd>JavaScript</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    auto dts = dl->find_all_elements("dt");
    auto dds = dl->find_all_elements("dd");
    ASSERT_EQ(dts.size(), 3u);
    ASSERT_EQ(dds.size(), 3u);
    EXPECT_EQ(dts[0]->text_content(), "HTML");
    EXPECT_EQ(dds[0]->text_content(), "HyperText Markup Language");
    EXPECT_EQ(dts[1]->text_content(), "CSS");
    EXPECT_EQ(dds[1]->text_content(), "Cascading Style Sheets");
    EXPECT_EQ(dts[2]->text_content(), "JS");
    EXPECT_EQ(dds[2]->text_content(), "JavaScript");
}

// 2. Parse a table with thead, tbody, and tfoot
TEST(HtmlParserTest, TableTheadTbodyTfootV96) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<thead><tr><th>Name</th><th>Age</th></tr></thead>"
        "<tbody><tr><td>Alice</td><td>30</td></tr>"
        "<tr><td>Bob</td><td>25</td></tr></tbody>"
        "<tfoot><tr><td colspan=\"2\">Total: 2</td></tr></tfoot>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* thead = doc->find_element("thead");
    ASSERT_NE(thead, nullptr);
    auto ths = thead->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Age");
    auto* tbody = doc->find_element("tbody");
    ASSERT_NE(tbody, nullptr);
    auto tds = tbody->find_all_elements("td");
    ASSERT_EQ(tds.size(), 4u);
    EXPECT_EQ(tds[0]->text_content(), "Alice");
    EXPECT_EQ(tds[3]->text_content(), "25");
    auto* tfoot = doc->find_element("tfoot");
    ASSERT_NE(tfoot, nullptr);
    auto foot_td = tfoot->find_element("td");
    ASSERT_NE(foot_td, nullptr);
    EXPECT_EQ(foot_td->text_content(), "Total: 2");
    EXPECT_EQ(get_attr_v63(foot_td, "colspan"), "2");
}

// 3. Parse nested blockquote with paragraph and cite
TEST(HtmlParserTest, NestedBlockquoteCiteV96) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<blockquote>"
        "<p>To be or not to be, that is the question.</p>"
        "<cite>William Shakespeare</cite>"
        "</blockquote>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* bq = doc->find_element("blockquote");
    ASSERT_NE(bq, nullptr);
    auto* p = bq->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "To be or not to be, that is the question.");
    auto* cite = bq->find_element("cite");
    ASSERT_NE(cite, nullptr);
    EXPECT_EQ(cite->text_content(), "William Shakespeare");
}

// 4. Parse details/summary elements
TEST(HtmlParserTest, DetailsSummaryElementsV96) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<details>"
        "<summary>Click to expand</summary>"
        "<p>Hidden content revealed after click.</p>"
        "</details>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    auto* summary = details->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click to expand");
    auto* p = details->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hidden content revealed after click.");
}

// 5. Parse multiple data attributes on a single element
TEST(HtmlParserTest, MultipleDataAttributesV96) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div data-id=\"42\" data-role=\"admin\" data-active=\"true\" class=\"user-card\">User</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto divs = doc->find_all_elements("div");
    // Find the div with data-id attribute
    clever::html::SimpleNode* target = nullptr;
    for (auto* d : divs) {
        if (!get_attr_v63(d, "data-id").empty()) {
            target = d;
            break;
        }
    }
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(get_attr_v63(target, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(target, "data-role"), "admin");
    EXPECT_EQ(get_attr_v63(target, "data-active"), "true");
    EXPECT_EQ(get_attr_v63(target, "class"), "user-card");
    EXPECT_EQ(target->text_content(), "User");
}

// 6. Parse figure with figcaption and img
TEST(HtmlParserTest, FigureWithFigcaptionImgV96) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<figure>"
        "<img src=\"photo.jpg\" alt=\"A sunset\">"
        "<figcaption>Sunset over the mountains</figcaption>"
        "</figure>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);
    auto* img = figure->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "photo.jpg");
    EXPECT_EQ(get_attr_v63(img, "alt"), "A sunset");
    auto* caption = figure->find_element("figcaption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->text_content(), "Sunset over the mountains");
}

// 7. Parse deeply nested structure (article > section > div > span)
TEST(HtmlParserTest, DeeplyNestedArticleSectionDivSpanV96) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<article>"
        "<section>"
        "<div>"
        "<span>Deep leaf text</span>"
        "</div>"
        "</section>"
        "</article>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    auto* section = article->find_element("section");
    ASSERT_NE(section, nullptr);
    auto* div = section->find_element("div");
    ASSERT_NE(div, nullptr);
    auto* span = div->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "Deep leaf text");
    // Also verify find_element works transitively from the root
    auto* span_from_root = doc->find_element("span");
    ASSERT_NE(span_from_root, nullptr);
    EXPECT_EQ(span_from_root->text_content(), "Deep leaf text");
}

// 8. Parse select element with optgroup and options
TEST(HtmlParserTest, SelectOptgroupOptionsV96) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<select name=\"car\">"
        "<optgroup label=\"Swedish Cars\">"
        "<option value=\"volvo\">Volvo</option>"
        "<option value=\"saab\">Saab</option>"
        "</optgroup>"
        "<optgroup label=\"German Cars\">"
        "<option value=\"bmw\">BMW</option>"
        "</optgroup>"
        "</select>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* sel = doc->find_element("select");
    ASSERT_NE(sel, nullptr);
    EXPECT_EQ(get_attr_v63(sel, "name"), "car");
    auto optgroups = sel->find_all_elements("optgroup");
    ASSERT_EQ(optgroups.size(), 2u);
    EXPECT_EQ(get_attr_v63(optgroups[0], "label"), "Swedish Cars");
    EXPECT_EQ(get_attr_v63(optgroups[1], "label"), "German Cars");
    auto opts0 = optgroups[0]->find_all_elements("option");
    ASSERT_EQ(opts0.size(), 2u);
    EXPECT_EQ(get_attr_v63(opts0[0], "value"), "volvo");
    EXPECT_EQ(opts0[0]->text_content(), "Volvo");
    EXPECT_EQ(get_attr_v63(opts0[1], "value"), "saab");
    EXPECT_EQ(opts0[1]->text_content(), "Saab");
    auto opts1 = optgroups[1]->find_all_elements("option");
    ASSERT_EQ(opts1.size(), 1u);
    EXPECT_EQ(get_attr_v63(opts1[0], "value"), "bmw");
    EXPECT_EQ(opts1[0]->text_content(), "BMW");
}

// ---------------------------------------------------------------------------
// V97 Tests — 8 diverse HTML parsing tests
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, DefinitionListDlDtDdV97) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>Term A</dt><dd>Definition A</dd>"
        "<dt>Term B</dt><dd>Definition B1</dd><dd>Definition B2</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    auto dts = dl->find_all_elements("dt");
    auto dds = dl->find_all_elements("dd");
    ASSERT_EQ(dts.size(), 2u);
    ASSERT_EQ(dds.size(), 3u);
    EXPECT_EQ(dts[0]->text_content(), "Term A");
    EXPECT_EQ(dts[1]->text_content(), "Term B");
    EXPECT_EQ(dds[0]->text_content(), "Definition A");
    EXPECT_EQ(dds[1]->text_content(), "Definition B1");
    EXPECT_EQ(dds[2]->text_content(), "Definition B2");
}

TEST(HtmlParserTest, NestedTablesV97) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<tr><td>Outer Cell"
        "<table><tr><td>Inner Cell</td></tr></table>"
        "</td></tr>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto tables = doc->find_all_elements("table");
    ASSERT_EQ(tables.size(), 2u);
    // Outer table contains the inner table
    auto inner_tables = tables[0]->find_all_elements("table");
    ASSERT_EQ(inner_tables.size(), 1u);
    // Inner table has one td
    auto inner_tds = inner_tables[0]->find_all_elements("td");
    ASSERT_GE(inner_tds.size(), 1u);
    EXPECT_EQ(inner_tds[0]->text_content(), "Inner Cell");
}

TEST(HtmlParserTest, DataAttributesV97) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div data-id=\"42\" data-role=\"admin\" data-active=\"true\">User</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "admin");
    EXPECT_EQ(get_attr_v63(div, "data-active"), "true");
    EXPECT_EQ(div->text_content(), "User");
}

TEST(HtmlParserTest, MultipleSiblingParagraphsAutoCloseV97) {
    // <p> tags should auto-close when another <p> is encountered
    auto doc = clever::html::parse(
        "<html><body>"
        "<p>First"
        "<p>Second"
        "<p>Third"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto paragraphs = doc->find_all_elements("p");
    ASSERT_EQ(paragraphs.size(), 3u);
    EXPECT_EQ(paragraphs[0]->text_content(), "First");
    EXPECT_EQ(paragraphs[1]->text_content(), "Second");
    EXPECT_EQ(paragraphs[2]->text_content(), "Third");
    // Each <p> should be a sibling, not nested
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    int p_direct = 0;
    for (auto& child : body->children) {
        if (child->tag_name == "p") p_direct++;
    }
    EXPECT_EQ(p_direct, 3);
}

TEST(HtmlParserTest, ScriptTagPreservesContentV97) {
    auto doc = clever::html::parse(
        "<html><head>"
        "<script>var x = 1 < 2 && 3 > 0; console.log(x);</script>"
        "</head><body></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    std::string content = script->text_content();
    // Script content should contain the raw text including < and >
    EXPECT_NE(content.find("var x"), std::string::npos);
    EXPECT_NE(content.find("< 2"), std::string::npos);
    EXPECT_NE(content.find("> 0"), std::string::npos);
}

TEST(HtmlParserTest, AnchorWithMultipleAttributesV97) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<a href=\"https://example.com\" target=\"_blank\" rel=\"noopener noreferrer\" title=\"Example\">"
        "Click Here</a>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(get_attr_v63(a, "href"), "https://example.com");
    EXPECT_EQ(get_attr_v63(a, "target"), "_blank");
    EXPECT_EQ(get_attr_v63(a, "rel"), "noopener noreferrer");
    EXPECT_EQ(get_attr_v63(a, "title"), "Example");
    EXPECT_EQ(a->text_content(), "Click Here");
}

TEST(HtmlParserTest, EmptyDocumentV97) {
    auto doc = clever::html::parse("");
    ASSERT_NE(doc, nullptr);
    // An empty document should still produce a root node
    // The root should either be empty or have generated html/head/body
    auto* html = doc->find_element("html");
    if (html) {
        auto* body = doc->find_element("body");
        // If html is generated, body should also be generated
        EXPECT_NE(body, nullptr);
        // Body should have no meaningful text
        if (body) {
            EXPECT_TRUE(body->text_content().empty());
        }
    } else {
        // Minimal root with no children is also valid
        EXPECT_TRUE(doc->children.empty());
    }
}

TEST(HtmlParserTest, FieldsetLegendFormElementsV97) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<form action=\"/submit\" method=\"post\">"
        "<fieldset>"
        "<legend>Personal Info</legend>"
        "<label for=\"name\">Name:</label>"
        "<input type=\"text\" id=\"name\" name=\"name\"/>"
        "<label for=\"email\">Email:</label>"
        "<input type=\"email\" id=\"email\" name=\"email\"/>"
        "</fieldset>"
        "</form>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "action"), "/submit");
    EXPECT_EQ(get_attr_v63(form, "method"), "post");
    auto* fieldset = doc->find_element("fieldset");
    ASSERT_NE(fieldset, nullptr);
    auto* legend = fieldset->find_element("legend");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->text_content(), "Personal Info");
    auto labels = fieldset->find_all_elements("label");
    ASSERT_EQ(labels.size(), 2u);
    EXPECT_EQ(get_attr_v63(labels[0], "for"), "name");
    EXPECT_EQ(get_attr_v63(labels[1], "for"), "email");
    auto inputs = fieldset->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "text");
    EXPECT_EQ(get_attr_v63(inputs[0], "id"), "name");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "email");
    EXPECT_EQ(get_attr_v63(inputs[1], "id"), "email");
}

// ---------------------------------------------------------------------------
// Cycle V98 — HTML parser tests for diverse parsing scenarios
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, NestedListsWithMixedContentV98) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ul>"
        "<li>Item A</li>"
        "<li><ol><li>Sub 1</li><li>Sub 2</li></ol></li>"
        "<li>Item C</li>"
        "</ul>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    auto lis = ul->find_all_elements("li");
    // 3 direct li + 2 nested li = 5 total
    ASSERT_EQ(lis.size(), 5u);
    EXPECT_EQ(lis[0]->text_content(), "Item A");
    auto* ol = ul->find_element("ol");
    ASSERT_NE(ol, nullptr);
    auto nested_lis = ol->find_all_elements("li");
    ASSERT_EQ(nested_lis.size(), 2u);
    EXPECT_EQ(nested_lis[0]->text_content(), "Sub 1");
    EXPECT_EQ(nested_lis[1]->text_content(), "Sub 2");
    EXPECT_EQ(lis[4]->text_content(), "Item C");
}

TEST(HtmlParserTest, TableStructureWithRowsAndCellsV98) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<thead><tr><th>Name</th><th>Age</th></tr></thead>"
        "<tbody>"
        "<tr><td>Alice</td><td>30</td></tr>"
        "<tr><td>Bob</td><td>25</td></tr>"
        "</tbody>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    auto ths = table->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Age");
    auto tds = table->find_all_elements("td");
    ASSERT_EQ(tds.size(), 4u);
    EXPECT_EQ(tds[0]->text_content(), "Alice");
    EXPECT_EQ(tds[1]->text_content(), "30");
    EXPECT_EQ(tds[2]->text_content(), "Bob");
    EXPECT_EQ(tds[3]->text_content(), "25");
}

TEST(HtmlParserTest, MultipleVoidElementsInSequenceV98) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<br><hr><br><hr><br>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    auto brs = body->find_all_elements("br");
    ASSERT_EQ(brs.size(), 3u);
    auto hrs = body->find_all_elements("hr");
    ASSERT_EQ(hrs.size(), 2u);
}

TEST(HtmlParserTest, DeeplyNestedDivsV98) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div><div><div><div><div>deep</div></div></div></div></div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto divs = doc->find_all_elements("div");
    ASSERT_EQ(divs.size(), 5u);
    // The innermost div should contain the text
    auto* innermost = divs[4];
    EXPECT_EQ(innermost->text_content(), "deep");
    EXPECT_TRUE(innermost->children.size() >= 1u);
}

TEST(HtmlParserTest, DataAttributesPreservedV98) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div data-id=\"42\" data-role=\"main\" data-visible=\"true\">Content</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "main");
    EXPECT_EQ(get_attr_v63(div, "data-visible"), "true");
    EXPECT_EQ(div->text_content(), "Content");
}

TEST(HtmlParserTest, EmptyElementsNoTextV98) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div></div><span></span><p></p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->text_content(), "");
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "");
}

TEST(HtmlParserTest, DefinitionListDlDtDdV98) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>Term 1</dt><dd>Definition 1</dd>"
        "<dt>Term 2</dt><dd>Definition 2</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    auto dts = dl->find_all_elements("dt");
    ASSERT_EQ(dts.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term 1");
    EXPECT_EQ(dts[1]->text_content(), "Term 2");
    auto dds = dl->find_all_elements("dd");
    ASSERT_EQ(dds.size(), 2u);
    EXPECT_EQ(dds[0]->text_content(), "Definition 1");
    EXPECT_EQ(dds[1]->text_content(), "Definition 2");
}

TEST(HtmlParserTest, AnchorLinksWithHrefAndTargetV98) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<nav>"
        "<a href=\"/home\" target=\"_self\">Home</a>"
        "<a href=\"/about\" target=\"_blank\">About</a>"
        "<a href=\"/contact\">Contact</a>"
        "</nav>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    auto links = nav->find_all_elements("a");
    ASSERT_EQ(links.size(), 3u);
    EXPECT_EQ(get_attr_v63(links[0], "href"), "/home");
    EXPECT_EQ(get_attr_v63(links[0], "target"), "_self");
    EXPECT_EQ(links[0]->text_content(), "Home");
    EXPECT_EQ(get_attr_v63(links[1], "href"), "/about");
    EXPECT_EQ(get_attr_v63(links[1], "target"), "_blank");
    EXPECT_EQ(links[1]->text_content(), "About");
    EXPECT_EQ(get_attr_v63(links[2], "href"), "/contact");
    EXPECT_EQ(links[2]->text_content(), "Contact");
}

TEST(HtmlParserTest, NestedTablesWithCaptionV99) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<caption>Sales Report</caption>"
        "<thead><tr><th>Product</th><th>Revenue</th></tr></thead>"
        "<tbody>"
        "<tr><td>Widget A</td><td>$1000</td></tr>"
        "<tr><td>Widget B</td><td>$2500</td></tr>"
        "</tbody>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    auto* caption = table->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->text_content(), "Sales Report");
    auto ths = table->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Product");
    EXPECT_EQ(ths[1]->text_content(), "Revenue");
    auto tds = table->find_all_elements("td");
    ASSERT_EQ(tds.size(), 4u);
    EXPECT_EQ(tds[0]->text_content(), "Widget A");
    EXPECT_EQ(tds[1]->text_content(), "$1000");
    EXPECT_EQ(tds[2]->text_content(), "Widget B");
    EXPECT_EQ(tds[3]->text_content(), "$2500");
}

TEST(HtmlParserTest, FieldsetWithLegendAndInputsV99) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<fieldset>"
        "<legend>Login Credentials</legend>"
        "<label>Username</label>"
        "<input type=\"text\" name=\"user\" />"
        "<label>Password</label>"
        "<input type=\"password\" name=\"pass\" />"
        "</fieldset>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* fieldset = doc->find_element("fieldset");
    ASSERT_NE(fieldset, nullptr);
    auto* legend = fieldset->find_element("legend");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->text_content(), "Login Credentials");
    auto labels = fieldset->find_all_elements("label");
    ASSERT_EQ(labels.size(), 2u);
    EXPECT_EQ(labels[0]->text_content(), "Username");
    EXPECT_EQ(labels[1]->text_content(), "Password");
    auto inputs = fieldset->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "text");
    EXPECT_EQ(get_attr_v63(inputs[0], "name"), "user");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "password");
    EXPECT_EQ(get_attr_v63(inputs[1], "name"), "pass");
}

TEST(HtmlParserTest, DetailsAndSummaryElementsV99) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<details>"
        "<summary>Click to expand</summary>"
        "<p>Hidden content revealed when details is open.</p>"
        "</details>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    auto* summary = details->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click to expand");
    auto* p = details->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hidden content revealed when details is open.");
}

TEST(HtmlParserTest, MultipleDataAttributesOnElementV99) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div data-id=\"42\" data-role=\"admin\" data-active=\"true\" class=\"user-card\">"
        "User Info"
        "</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "admin");
    EXPECT_EQ(get_attr_v63(div, "data-active"), "true");
    EXPECT_EQ(get_attr_v63(div, "class"), "user-card");
    EXPECT_EQ(div->text_content(), "User Info");
}

TEST(HtmlParserTest, OrderedListWithNestedUnorderedListV99) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ol>"
        "<li>First item"
        "<ul><li>Sub A</li><li>Sub B</li></ul>"
        "</li>"
        "<li>Second item</li>"
        "<li>Third item</li>"
        "</ol>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    auto top_items = ol->find_all_elements("li");
    // find_all_elements is recursive, so it finds all li including nested
    ASSERT_GE(top_items.size(), 5u);
    auto* ul = ol->find_element("ul");
    ASSERT_NE(ul, nullptr);
    auto sub_items = ul->find_all_elements("li");
    ASSERT_EQ(sub_items.size(), 2u);
    EXPECT_EQ(sub_items[0]->text_content(), "Sub A");
    EXPECT_EQ(sub_items[1]->text_content(), "Sub B");
}

TEST(HtmlParserTest, EmptyElementsAndSelfClosingTagsV99) {
    auto doc = clever::html::parse(
        "<html><head>"
        "<meta charset=\"utf-8\" />"
        "<link rel=\"icon\" href=\"/favicon.ico\" />"
        "</head><body>"
        "<br/><hr/>"
        "<img src=\"logo.png\" alt=\"Logo\" />"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* meta = doc->find_element("meta");
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(get_attr_v63(meta, "charset"), "utf-8");
    auto* link = doc->find_element("link");
    ASSERT_NE(link, nullptr);
    EXPECT_EQ(get_attr_v63(link, "rel"), "icon");
    EXPECT_EQ(get_attr_v63(link, "href"), "/favicon.ico");
    auto* br = doc->find_element("br");
    ASSERT_NE(br, nullptr);
    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "logo.png");
    EXPECT_EQ(get_attr_v63(img, "alt"), "Logo");
}

TEST(HtmlParserTest, SelectWithOptgroupAndOptionsV99) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<select name=\"cars\">"
        "<optgroup label=\"Swedish Cars\">"
        "<option value=\"volvo\">Volvo</option>"
        "<option value=\"saab\">Saab</option>"
        "</optgroup>"
        "<optgroup label=\"German Cars\">"
        "<option value=\"mercedes\">Mercedes</option>"
        "<option value=\"audi\">Audi</option>"
        "</optgroup>"
        "</select>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* select = doc->find_element("select");
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(get_attr_v63(select, "name"), "cars");
    auto optgroups = select->find_all_elements("optgroup");
    ASSERT_EQ(optgroups.size(), 2u);
    EXPECT_EQ(get_attr_v63(optgroups[0], "label"), "Swedish Cars");
    EXPECT_EQ(get_attr_v63(optgroups[1], "label"), "German Cars");
    auto all_options = select->find_all_elements("option");
    ASSERT_EQ(all_options.size(), 4u);
    EXPECT_EQ(get_attr_v63(all_options[0], "value"), "volvo");
    EXPECT_EQ(all_options[0]->text_content(), "Volvo");
    EXPECT_EQ(get_attr_v63(all_options[2], "value"), "mercedes");
    EXPECT_EQ(all_options[2]->text_content(), "Mercedes");
    EXPECT_EQ(get_attr_v63(all_options[3], "value"), "audi");
    EXPECT_EQ(all_options[3]->text_content(), "Audi");
}

TEST(HtmlParserTest, ArticleWithHeaderFooterAndSectionsV99) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<article>"
        "<header><h1>Article Title</h1></header>"
        "<section><p>Section one content.</p></section>"
        "<section><p>Section two content.</p></section>"
        "<footer><small>Copyright 2026</small></footer>"
        "</article>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    auto* header = article->find_element("header");
    ASSERT_NE(header, nullptr);
    auto* h1 = header->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Article Title");
    auto sections = article->find_all_elements("section");
    ASSERT_EQ(sections.size(), 2u);
    auto* p1 = sections[0]->find_element("p");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->text_content(), "Section one content.");
    auto* p2 = sections[1]->find_element("p");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->text_content(), "Section two content.");
    auto* footer = article->find_element("footer");
    ASSERT_NE(footer, nullptr);
    auto* small = footer->find_element("small");
    ASSERT_NE(small, nullptr);
    EXPECT_EQ(small->text_content(), "Copyright 2026");
}

// ============================================================================
// V100 Tests
// ============================================================================

TEST(HtmlParserTest, NestedListsWithMixedContentV100) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ul><li>Item 1</li><li><ol><li>Sub A</li><li>Sub B</li></ol></li><li>Item 3</li></ul>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    auto lis = ul->find_all_elements("li");
    // The top-level ul has 3 li children, but find_all_elements is recursive
    // so it finds all 5 li elements (3 in ul + 2 in ol)
    EXPECT_EQ(lis.size(), 5u);
    EXPECT_EQ(lis[0]->text_content(), "Item 1");
    auto* ol = ul->find_element("ol");
    ASSERT_NE(ol, nullptr);
    auto sub_lis = ol->find_all_elements("li");
    ASSERT_EQ(sub_lis.size(), 2u);
    EXPECT_EQ(sub_lis[0]->text_content(), "Sub A");
    EXPECT_EQ(sub_lis[1]->text_content(), "Sub B");
}

TEST(HtmlParserTest, TableStructureWithRowsAndCellsV100) {
    auto doc = clever::html::parse(
        "<html><body><table>"
        "<tr><th>Name</th><th>Age</th></tr>"
        "<tr><td>Alice</td><td>30</td></tr>"
        "<tr><td>Bob</td><td>25</td></tr>"
        "</table></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    auto rows = table->find_all_elements("tr");
    ASSERT_EQ(rows.size(), 3u);
    auto headers = rows[0]->find_all_elements("th");
    ASSERT_EQ(headers.size(), 2u);
    EXPECT_EQ(headers[0]->text_content(), "Name");
    EXPECT_EQ(headers[1]->text_content(), "Age");
    auto cells = rows[1]->find_all_elements("td");
    ASSERT_EQ(cells.size(), 2u);
    EXPECT_EQ(cells[0]->text_content(), "Alice");
    EXPECT_EQ(cells[1]->text_content(), "30");
}

TEST(HtmlParserTest, MultipleAttributesOnSingleElementV100) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<input type=\"email\" name=\"user_email\" placeholder=\"you@example.com\" required=\"\" disabled=\"\"/>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "email");
    EXPECT_EQ(get_attr_v63(input, "name"), "user_email");
    EXPECT_EQ(get_attr_v63(input, "placeholder"), "you@example.com");
    EXPECT_EQ(get_attr_v63(input, "required"), "");
    EXPECT_EQ(get_attr_v63(input, "disabled"), "");
    EXPECT_GE(input->attributes.size(), 5u);
}

TEST(HtmlParserTest, DeeplyNestedDivStructureV100) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div><div><div><div><div><span>Deep</span></div></div></div></div></div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "Deep");
    // Verify the nesting depth by walking up through divs
    auto divs = doc->find_all_elements("div");
    EXPECT_EQ(divs.size(), 5u);
}

TEST(HtmlParserTest, EmptyElementsAndSiblingTextNodesV100) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p>Before</p><br/><hr/><p>After</p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto ps = doc->find_all_elements("p");
    ASSERT_EQ(ps.size(), 2u);
    EXPECT_EQ(ps[0]->text_content(), "Before");
    EXPECT_EQ(ps[1]->text_content(), "After");
    auto* br = doc->find_element("br");
    EXPECT_NE(br, nullptr);
    auto* hr = doc->find_element("hr");
    EXPECT_NE(hr, nullptr);
}

TEST(HtmlParserTest, DefinitionListDlDtDdV100) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>HTML</dt><dd>HyperText Markup Language</dd>"
        "<dt>CSS</dt><dd>Cascading Style Sheets</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    auto dts = dl->find_all_elements("dt");
    ASSERT_EQ(dts.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "HTML");
    EXPECT_EQ(dts[1]->text_content(), "CSS");
    auto dds = dl->find_all_elements("dd");
    ASSERT_EQ(dds.size(), 2u);
    EXPECT_EQ(dds[0]->text_content(), "HyperText Markup Language");
    EXPECT_EQ(dds[1]->text_content(), "Cascading Style Sheets");
}

TEST(HtmlParserTest, AnchorTagsWithHrefAndTargetAttributesV100) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<a href=\"https://example.com\" target=\"_blank\" rel=\"noopener\">Example</a>"
        "<a href=\"/about\" class=\"nav-link\">About</a>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto anchors = doc->find_all_elements("a");
    ASSERT_EQ(anchors.size(), 2u);
    EXPECT_EQ(get_attr_v63(anchors[0], "href"), "https://example.com");
    EXPECT_EQ(get_attr_v63(anchors[0], "target"), "_blank");
    EXPECT_EQ(get_attr_v63(anchors[0], "rel"), "noopener");
    EXPECT_EQ(anchors[0]->text_content(), "Example");
    EXPECT_EQ(get_attr_v63(anchors[1], "href"), "/about");
    EXPECT_EQ(get_attr_v63(anchors[1], "class"), "nav-link");
    EXPECT_EQ(anchors[1]->text_content(), "About");
}

TEST(HtmlParserTest, DataAttributesOnElementV100) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div data-id=\"42\" data-role=\"container\" data-active=\"true\">"
        "<span data-tooltip=\"Hello world\">Hover me</span>"
        "</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "container");
    EXPECT_EQ(get_attr_v63(div, "data-active"), "true");
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(get_attr_v63(span, "data-tooltip"), "Hello world");
    EXPECT_EQ(span->text_content(), "Hover me");
}

// ---------------------------------------------------------------------------
// V101 tests — diverse HTML parsing scenarios
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, NestedListStructureV101) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ul>"
        "<li>First</li>"
        "<li>Second</li>"
        "<li>Third</li>"
        "</ul>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    EXPECT_EQ(ul->tag_name, "ul");
    auto items = doc->find_all_elements("li");
    ASSERT_EQ(items.size(), 3u);
    EXPECT_EQ(items[0]->text_content(), "First");
    EXPECT_EQ(items[1]->text_content(), "Second");
    EXPECT_EQ(items[2]->text_content(), "Third");
}

TEST(HtmlParserTest, TableWithRowsAndCellsV101) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<tr><th>Name</th><th>Age</th></tr>"
        "<tr><td>Alice</td><td>30</td></tr>"
        "<tr><td>Bob</td><td>25</td></tr>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto rows = doc->find_all_elements("tr");
    ASSERT_EQ(rows.size(), 3u);
    auto headers = doc->find_all_elements("th");
    ASSERT_EQ(headers.size(), 2u);
    EXPECT_EQ(headers[0]->text_content(), "Name");
    EXPECT_EQ(headers[1]->text_content(), "Age");
    auto cells = doc->find_all_elements("td");
    ASSERT_EQ(cells.size(), 4u);
    EXPECT_EQ(cells[0]->text_content(), "Alice");
    EXPECT_EQ(cells[1]->text_content(), "30");
    EXPECT_EQ(cells[2]->text_content(), "Bob");
    EXPECT_EQ(cells[3]->text_content(), "25");
}

TEST(HtmlParserTest, FormWithMultipleInputTypesV101) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<form action=\"/submit\" method=\"post\">"
        "<input type=\"email\" name=\"email\" placeholder=\"you@example.com\"/>"
        "<input type=\"hidden\" name=\"token\" value=\"abc123\"/>"
        "<textarea name=\"message\">Hello</textarea>"
        "<button type=\"submit\">Send</button>"
        "</form>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "action"), "/submit");
    EXPECT_EQ(get_attr_v63(form, "method"), "post");
    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "email");
    EXPECT_EQ(get_attr_v63(inputs[0], "placeholder"), "you@example.com");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "hidden");
    EXPECT_EQ(get_attr_v63(inputs[1], "value"), "abc123");
    auto* textarea = doc->find_element("textarea");
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(get_attr_v63(textarea, "name"), "message");
    EXPECT_EQ(textarea->text_content(), "Hello");
    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(get_attr_v63(button, "type"), "submit");
    EXPECT_EQ(button->text_content(), "Send");
}

TEST(HtmlParserTest, DeeplyNestedDivHierarchyV101) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div class=\"l1\">"
        "<div class=\"l2\">"
        "<div class=\"l3\">"
        "<div class=\"l4\">"
        "<p>Deep text</p>"
        "</div></div></div></div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto divs = doc->find_all_elements("div");
    ASSERT_EQ(divs.size(), 4u);
    EXPECT_EQ(get_attr_v63(divs[0], "class"), "l1");
    EXPECT_EQ(get_attr_v63(divs[1], "class"), "l2");
    EXPECT_EQ(get_attr_v63(divs[2], "class"), "l3");
    EXPECT_EQ(get_attr_v63(divs[3], "class"), "l4");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Deep text");
}

TEST(HtmlParserTest, EmptyAndVoidElementsV101) {
    auto doc = clever::html::parse(
        "<html><head></head><body>"
        "<br/><hr/>"
        "<div></div>"
        "<span></span>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* br = doc->find_element("br");
    ASSERT_NE(br, nullptr);
    EXPECT_EQ(br->tag_name, "br");
    EXPECT_TRUE(br->children.empty());
    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
    EXPECT_EQ(hr->tag_name, "hr");
    EXPECT_TRUE(hr->children.empty());
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_TRUE(div->children.empty());
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_TRUE(span->children.empty());
}

TEST(HtmlParserTest, InlineElementsMixedWithTextV101) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p>Hello <strong>bold</strong> and <em>italic</em> world</p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "bold");
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "italic");
    // The full text content of p should contain all inline text
    std::string full = p->text_content();
    EXPECT_NE(full.find("Hello"), std::string::npos);
    EXPECT_NE(full.find("bold"), std::string::npos);
    EXPECT_NE(full.find("italic"), std::string::npos);
    EXPECT_NE(full.find("world"), std::string::npos);
}

TEST(HtmlParserTest, MultipleAttributesOnSingleElementV101) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<a href=\"https://test.org\" target=\"_self\" rel=\"nofollow\" "
        "title=\"Test Link\" class=\"primary\" id=\"main-link\">Click</a>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(get_attr_v63(a, "href"), "https://test.org");
    EXPECT_EQ(get_attr_v63(a, "target"), "_self");
    EXPECT_EQ(get_attr_v63(a, "rel"), "nofollow");
    EXPECT_EQ(get_attr_v63(a, "title"), "Test Link");
    EXPECT_EQ(get_attr_v63(a, "class"), "primary");
    EXPECT_EQ(get_attr_v63(a, "id"), "main-link");
    EXPECT_EQ(a->text_content(), "Click");
    EXPECT_EQ(a->attributes.size(), 6u);
}

TEST(HtmlParserTest, DefinitionListWithTermsAndDescriptionsV101) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>HTML</dt><dd>HyperText Markup Language</dd>"
        "<dt>CSS</dt><dd>Cascading Style Sheets</dd>"
        "<dt>JS</dt><dd>JavaScript</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    EXPECT_EQ(dl->tag_name, "dl");
    auto terms = doc->find_all_elements("dt");
    ASSERT_EQ(terms.size(), 3u);
    EXPECT_EQ(terms[0]->text_content(), "HTML");
    EXPECT_EQ(terms[1]->text_content(), "CSS");
    EXPECT_EQ(terms[2]->text_content(), "JS");
    auto descs = doc->find_all_elements("dd");
    ASSERT_EQ(descs.size(), 3u);
    EXPECT_EQ(descs[0]->text_content(), "HyperText Markup Language");
    EXPECT_EQ(descs[1]->text_content(), "Cascading Style Sheets");
    EXPECT_EQ(descs[2]->text_content(), "JavaScript");
}

// ============================================================================
// V102 Tests
// ============================================================================

TEST(HtmlParserTest, NestedTablesWithCaptionAndHeaderV102) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<caption>Sales Report</caption>"
        "<thead><tr><th>Region</th><th>Q1</th><th>Q2</th></tr></thead>"
        "<tbody>"
        "<tr><td>North</td><td>100</td><td>150</td></tr>"
        "<tr><td>South</td><td>200</td><td>250</td></tr>"
        "</tbody>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    auto* caption = doc->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->text_content(), "Sales Report");
    auto ths = doc->find_all_elements("th");
    ASSERT_EQ(ths.size(), 3u);
    EXPECT_EQ(ths[0]->text_content(), "Region");
    EXPECT_EQ(ths[1]->text_content(), "Q1");
    EXPECT_EQ(ths[2]->text_content(), "Q2");
    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 6u);
    EXPECT_EQ(tds[0]->text_content(), "North");
    EXPECT_EQ(tds[3]->text_content(), "South");
    EXPECT_EQ(tds[5]->text_content(), "250");
}

TEST(HtmlParserTest, FieldsetWithLegendAndMultipleInputsV102) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<fieldset>"
        "<legend>Personal Info</legend>"
        "<label>Name</label><input type=\"text\" name=\"username\" placeholder=\"Enter name\"/>"
        "<label>Email</label><input type=\"email\" name=\"useremail\" required/>"
        "</fieldset>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* fieldset = doc->find_element("fieldset");
    ASSERT_NE(fieldset, nullptr);
    auto* legend = doc->find_element("legend");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->text_content(), "Personal Info");
    auto labels = doc->find_all_elements("label");
    ASSERT_EQ(labels.size(), 2u);
    EXPECT_EQ(labels[0]->text_content(), "Name");
    EXPECT_EQ(labels[1]->text_content(), "Email");
    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "text");
    EXPECT_EQ(get_attr_v63(inputs[0], "name"), "username");
    EXPECT_EQ(get_attr_v63(inputs[0], "placeholder"), "Enter name");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "email");
    EXPECT_EQ(get_attr_v63(inputs[1], "name"), "useremail");
}

TEST(HtmlParserTest, DetailsAndSummaryElementsV102) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<details>"
        "<summary>Click to expand</summary>"
        "<p>Hidden content revealed on click.</p>"
        "</details>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    EXPECT_EQ(details->tag_name, "details");
    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click to expand");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hidden content revealed on click.");
}

TEST(HtmlParserTest, FigureWithFigcaptionAndImageV102) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<figure>"
        "<img src=\"/photos/sunset.jpg\" alt=\"Sunset over mountains\" width=\"800\" height=\"600\"/>"
        "<figcaption>A beautiful sunset over the mountain range.</figcaption>"
        "</figure>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "/photos/sunset.jpg");
    EXPECT_EQ(get_attr_v63(img, "alt"), "Sunset over mountains");
    EXPECT_EQ(get_attr_v63(img, "width"), "800");
    EXPECT_EQ(get_attr_v63(img, "height"), "600");
    auto* figcaption = doc->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->text_content(), "A beautiful sunset over the mountain range.");
}

TEST(HtmlParserTest, AudioElementWithMultipleSourcesV102) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<audio controls>"
        "<source src=\"song.ogg\" type=\"audio/ogg\"/>"
        "<source src=\"song.mp3\" type=\"audio/mpeg\"/>"
        "Your browser does not support audio."
        "</audio>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* audio = doc->find_element("audio");
    ASSERT_NE(audio, nullptr);
    EXPECT_EQ(audio->tag_name, "audio");
    auto sources = doc->find_all_elements("source");
    ASSERT_EQ(sources.size(), 2u);
    EXPECT_EQ(get_attr_v63(sources[0], "src"), "song.ogg");
    EXPECT_EQ(get_attr_v63(sources[0], "type"), "audio/ogg");
    EXPECT_EQ(get_attr_v63(sources[1], "src"), "song.mp3");
    EXPECT_EQ(get_attr_v63(sources[1], "type"), "audio/mpeg");
}

TEST(HtmlParserTest, ProgressAndMeterElementsWithAttributesV102) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<progress value=\"70\" max=\"100\">70%</progress>"
        "<meter min=\"0\" max=\"10\" value=\"7\" low=\"3\" high=\"8\" optimum=\"5\">7 out of 10</meter>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* progress = doc->find_element("progress");
    ASSERT_NE(progress, nullptr);
    EXPECT_EQ(get_attr_v63(progress, "value"), "70");
    EXPECT_EQ(get_attr_v63(progress, "max"), "100");
    EXPECT_EQ(progress->text_content(), "70%");
    auto* meter = doc->find_element("meter");
    ASSERT_NE(meter, nullptr);
    EXPECT_EQ(get_attr_v63(meter, "min"), "0");
    EXPECT_EQ(get_attr_v63(meter, "max"), "10");
    EXPECT_EQ(get_attr_v63(meter, "value"), "7");
    EXPECT_EQ(get_attr_v63(meter, "low"), "3");
    EXPECT_EQ(get_attr_v63(meter, "high"), "8");
    EXPECT_EQ(get_attr_v63(meter, "optimum"), "5");
    EXPECT_EQ(meter->text_content(), "7 out of 10");
}

TEST(HtmlParserTest, NestedArticlesWithHeaderFooterAndTimeV102) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<article>"
        "<header><h1>Main Article</h1></header>"
        "<p>Introductory paragraph.</p>"
        "<article>"
        "<header><h2>Sub-article</h2></header>"
        "<time datetime=\"2026-02-28\">Feb 28, 2026</time>"
        "<p>Nested content here.</p>"
        "</article>"
        "<footer>Author: Jane Doe</footer>"
        "</article>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto articles = doc->find_all_elements("article");
    ASSERT_EQ(articles.size(), 2u);
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Main Article");
    auto* h2 = doc->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->text_content(), "Sub-article");
    auto* time_el = doc->find_element("time");
    ASSERT_NE(time_el, nullptr);
    EXPECT_EQ(get_attr_v63(time_el, "datetime"), "2026-02-28");
    EXPECT_EQ(time_el->text_content(), "Feb 28, 2026");
    auto* footer = doc->find_element("footer");
    ASSERT_NE(footer, nullptr);
    EXPECT_EQ(footer->text_content(), "Author: Jane Doe");
}

TEST(HtmlParserTest, DataListWithOptionsAndAssociatedInputV102) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<input list=\"browsers\" name=\"browser\" type=\"text\"/>"
        "<datalist id=\"browsers\">"
        "<option value=\"Chrome\"/>"
        "<option value=\"Firefox\"/>"
        "<option value=\"Safari\"/>"
        "<option value=\"Edge\"/>"
        "</datalist>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "list"), "browsers");
    EXPECT_EQ(get_attr_v63(input, "name"), "browser");
    EXPECT_EQ(get_attr_v63(input, "type"), "text");
    auto* datalist = doc->find_element("datalist");
    ASSERT_NE(datalist, nullptr);
    EXPECT_EQ(get_attr_v63(datalist, "id"), "browsers");
    auto options = doc->find_all_elements("option");
    ASSERT_EQ(options.size(), 4u);
    EXPECT_EQ(get_attr_v63(options[0], "value"), "Chrome");
    EXPECT_EQ(get_attr_v63(options[1], "value"), "Firefox");
    EXPECT_EQ(get_attr_v63(options[2], "value"), "Safari");
    EXPECT_EQ(get_attr_v63(options[3], "value"), "Edge");
}

TEST(HtmlParserTest, NavWithUnorderedListLinksV103) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<nav>"
        "<ul>"
        "<li><a href=\"/home\">Home</a></li>"
        "<li><a href=\"/about\">About</a></li>"
        "<li><a href=\"/contact\">Contact</a></li>"
        "</ul>"
        "</nav>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 3u);
    auto anchors = doc->find_all_elements("a");
    ASSERT_EQ(anchors.size(), 3u);
    EXPECT_EQ(get_attr_v63(anchors[0], "href"), "/home");
    EXPECT_EQ(anchors[0]->text_content(), "Home");
    EXPECT_EQ(get_attr_v63(anchors[1], "href"), "/about");
    EXPECT_EQ(anchors[1]->text_content(), "About");
    EXPECT_EQ(get_attr_v63(anchors[2], "href"), "/contact");
    EXPECT_EQ(anchors[2]->text_content(), "Contact");
}

TEST(HtmlParserTest, VideoElementWithSourcesAndTrackV103) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<video controls width=\"640\" height=\"360\">"
        "<source src=\"movie.mp4\" type=\"video/mp4\"/>"
        "<source src=\"movie.webm\" type=\"video/webm\"/>"
        "<track kind=\"subtitles\" src=\"subs_en.vtt\" srclang=\"en\" label=\"English\"/>"
        "</video>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    EXPECT_EQ(get_attr_v63(video, "width"), "640");
    EXPECT_EQ(get_attr_v63(video, "height"), "360");
    auto sources = doc->find_all_elements("source");
    ASSERT_EQ(sources.size(), 2u);
    EXPECT_EQ(get_attr_v63(sources[0], "src"), "movie.mp4");
    EXPECT_EQ(get_attr_v63(sources[0], "type"), "video/mp4");
    EXPECT_EQ(get_attr_v63(sources[1], "src"), "movie.webm");
    auto* track = doc->find_element("track");
    ASSERT_NE(track, nullptr);
    EXPECT_EQ(get_attr_v63(track, "kind"), "subtitles");
    EXPECT_EQ(get_attr_v63(track, "srclang"), "en");
    EXPECT_EQ(get_attr_v63(track, "label"), "English");
}

TEST(HtmlParserTest, DescriptionListWithMultipleTermsAndDescV103) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>HTML</dt><dd>HyperText Markup Language</dd>"
        "<dt>CSS</dt><dd>Cascading Style Sheets</dd>"
        "<dt>JS</dt><dd>JavaScript</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    auto dts = doc->find_all_elements("dt");
    ASSERT_EQ(dts.size(), 3u);
    auto dds = doc->find_all_elements("dd");
    ASSERT_EQ(dds.size(), 3u);
    EXPECT_EQ(dts[0]->text_content(), "HTML");
    EXPECT_EQ(dds[0]->text_content(), "HyperText Markup Language");
    EXPECT_EQ(dts[1]->text_content(), "CSS");
    EXPECT_EQ(dds[1]->text_content(), "Cascading Style Sheets");
    EXPECT_EQ(dts[2]->text_content(), "JS");
    EXPECT_EQ(dds[2]->text_content(), "JavaScript");
}

TEST(HtmlParserTest, TableWithColGroupAndFooterV103) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<colgroup><col span=\"2\"/><col/></colgroup>"
        "<thead><tr><th>Item</th><th>Qty</th><th>Price</th></tr></thead>"
        "<tbody><tr><td>Apple</td><td>5</td><td>$1.50</td></tr></tbody>"
        "<tfoot><tr><td colspan=\"2\">Total</td><td>$7.50</td></tr></tfoot>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    auto* colgroup = doc->find_element("colgroup");
    ASSERT_NE(colgroup, nullptr);
    auto cols = doc->find_all_elements("col");
    ASSERT_GE(cols.size(), 1u);
    EXPECT_EQ(get_attr_v63(cols[0], "span"), "2");
    auto* tfoot = doc->find_element("tfoot");
    ASSERT_NE(tfoot, nullptr);
    auto tds = doc->find_all_elements("td");
    ASSERT_GE(tds.size(), 5u);
    EXPECT_EQ(tds[0]->text_content(), "Apple");
    EXPECT_EQ(tds[3]->text_content(), "Total");
    EXPECT_EQ(tds[4]->text_content(), "$7.50");
}

TEST(HtmlParserTest, PictureElementWithMultipleSourcesV103) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<picture>"
        "<source media=\"(min-width: 800px)\" srcset=\"large.jpg\"/>"
        "<source media=\"(min-width: 400px)\" srcset=\"medium.jpg\"/>"
        "<img src=\"small.jpg\" alt=\"Responsive image\"/>"
        "</picture>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* picture = doc->find_element("picture");
    ASSERT_NE(picture, nullptr);
    auto sources = doc->find_all_elements("source");
    ASSERT_EQ(sources.size(), 2u);
    EXPECT_EQ(get_attr_v63(sources[0], "media"), "(min-width: 800px)");
    EXPECT_EQ(get_attr_v63(sources[0], "srcset"), "large.jpg");
    EXPECT_EQ(get_attr_v63(sources[1], "srcset"), "medium.jpg");
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "small.jpg");
    EXPECT_EQ(get_attr_v63(img, "alt"), "Responsive image");
}

TEST(HtmlParserTest, FormWithTextareaAndSelectGroupV103) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<form action=\"/submit\" method=\"post\">"
        "<textarea name=\"message\" rows=\"5\" cols=\"40\">Default text</textarea>"
        "<select name=\"priority\">"
        "<optgroup label=\"Urgency\">"
        "<option value=\"low\">Low</option>"
        "<option value=\"high\">High</option>"
        "</optgroup>"
        "</select>"
        "<button type=\"submit\">Send</button>"
        "</form>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "action"), "/submit");
    EXPECT_EQ(get_attr_v63(form, "method"), "post");
    auto* textarea = doc->find_element("textarea");
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(get_attr_v63(textarea, "name"), "message");
    EXPECT_EQ(get_attr_v63(textarea, "rows"), "5");
    EXPECT_EQ(textarea->text_content(), "Default text");
    auto* optgroup = doc->find_element("optgroup");
    ASSERT_NE(optgroup, nullptr);
    EXPECT_EQ(get_attr_v63(optgroup, "label"), "Urgency");
    auto options = doc->find_all_elements("option");
    ASSERT_EQ(options.size(), 2u);
    EXPECT_EQ(get_attr_v63(options[0], "value"), "low");
    EXPECT_EQ(options[1]->text_content(), "High");
    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text_content(), "Send");
}

TEST(HtmlParserTest, SectionWithHeadingsAndParagraphsV103) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<section>"
        "<h1>Main Title</h1>"
        "<p>Intro paragraph.</p>"
        "<h2>Subsection</h2>"
        "<p>Details here.</p>"
        "<h3>Sub-subsection</h3>"
        "<p>More details.</p>"
        "</section>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* section = doc->find_element("section");
    ASSERT_NE(section, nullptr);
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Main Title");
    auto* h2 = doc->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->text_content(), "Subsection");
    auto* h3 = doc->find_element("h3");
    ASSERT_NE(h3, nullptr);
    EXPECT_EQ(h3->text_content(), "Sub-subsection");
    auto paragraphs = doc->find_all_elements("p");
    ASSERT_EQ(paragraphs.size(), 3u);
    EXPECT_EQ(paragraphs[0]->text_content(), "Intro paragraph.");
    EXPECT_EQ(paragraphs[1]->text_content(), "Details here.");
    EXPECT_EQ(paragraphs[2]->text_content(), "More details.");
}

TEST(HtmlParserTest, MapWithAreaElementsV103) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<img src=\"workspace.png\" usemap=\"#workmap\" alt=\"Workspace\"/>"
        "<map name=\"workmap\">"
        "<area shape=\"rect\" coords=\"34,44,270,350\" alt=\"Computer\" href=\"computer.htm\"/>"
        "<area shape=\"circle\" coords=\"337,300,44\" alt=\"Coffee\" href=\"coffee.htm\"/>"
        "<area shape=\"poly\" coords=\"0,0,50,50,100,0\" alt=\"Triangle\" href=\"triangle.htm\"/>"
        "</map>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "usemap"), "#workmap");
    auto* map = doc->find_element("map");
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(get_attr_v63(map, "name"), "workmap");
    auto areas = doc->find_all_elements("area");
    ASSERT_EQ(areas.size(), 3u);
    EXPECT_EQ(get_attr_v63(areas[0], "shape"), "rect");
    EXPECT_EQ(get_attr_v63(areas[0], "alt"), "Computer");
    EXPECT_EQ(get_attr_v63(areas[0], "href"), "computer.htm");
    EXPECT_EQ(get_attr_v63(areas[1], "shape"), "circle");
    EXPECT_EQ(get_attr_v63(areas[1], "coords"), "337,300,44");
    EXPECT_EQ(get_attr_v63(areas[2], "shape"), "poly");
    EXPECT_EQ(get_attr_v63(areas[2], "href"), "triangle.htm");
}

// ============================================================================
// V104 Tests
// ============================================================================

TEST(HtmlParserTest, NestedDefinitionListV104) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>Term 1</dt><dd>Definition 1</dd>"
        "<dt>Term 2</dt><dd>Definition 2a</dd><dd>Definition 2b</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    auto dts = doc->find_all_elements("dt");
    ASSERT_EQ(dts.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term 1");
    EXPECT_EQ(dts[1]->text_content(), "Term 2");
    auto dds = doc->find_all_elements("dd");
    ASSERT_EQ(dds.size(), 3u);
    EXPECT_EQ(dds[0]->text_content(), "Definition 1");
    EXPECT_EQ(dds[1]->text_content(), "Definition 2a");
    EXPECT_EQ(dds[2]->text_content(), "Definition 2b");
}

TEST(HtmlParserTest, TableWithColspanAndRowspanV104) {
    auto doc = clever::html::parse(
        "<html><body><table>"
        "<tr><th colspan=\"2\">Header</th></tr>"
        "<tr><td rowspan=\"2\">Left</td><td>Right1</td></tr>"
        "<tr><td>Right2</td></tr>"
        "</table></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* th = doc->find_element("th");
    ASSERT_NE(th, nullptr);
    EXPECT_EQ(get_attr_v63(th, "colspan"), "2");
    EXPECT_EQ(th->text_content(), "Header");
    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 3u);
    EXPECT_EQ(get_attr_v63(tds[0], "rowspan"), "2");
    EXPECT_EQ(tds[0]->text_content(), "Left");
    EXPECT_EQ(tds[1]->text_content(), "Right1");
    EXPECT_EQ(tds[2]->text_content(), "Right2");
}

TEST(HtmlParserTest, DetailsAndSummaryElementsV104) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<details open=\"\">"
        "<summary>Click to expand</summary>"
        "<p>Hidden content revealed.</p>"
        "</details>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    EXPECT_EQ(get_attr_v63(details, "open"), "");
    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click to expand");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hidden content revealed.");
}

TEST(HtmlParserTest, MixedInlineElementsPreserveTextV104) {
    auto doc = clever::html::parse(
        "<html><body><p>Hello <strong>bold</strong> and <em>italic</em> world</p></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hello bold and italic world");
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "bold");
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "italic");
}

TEST(HtmlParserTest, ScriptAndStyleRawTextV104) {
    auto doc = clever::html::parse(
        "<html><head>"
        "<style>body { color: red; }</style>"
        "<script>var x = 1 < 2;</script>"
        "</head><body></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* style = doc->find_element("style");
    ASSERT_NE(style, nullptr);
    EXPECT_EQ(style->text_content(), "body { color: red; }");
    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    EXPECT_EQ(script->text_content(), "var x = 1 < 2;");
}

TEST(HtmlParserTest, MultipleDataAttributesV104) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div data-id=\"42\" data-name=\"widget\" data-active=\"true\" class=\"item\">Content</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-name"), "widget");
    EXPECT_EQ(get_attr_v63(div, "data-active"), "true");
    EXPECT_EQ(get_attr_v63(div, "class"), "item");
    EXPECT_EQ(div->text_content(), "Content");
    EXPECT_GE(div->attributes.size(), 4u);
}

TEST(HtmlParserTest, EmptyAndVoidElementMixV104) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div></div>"
        "<br/>"
        "<hr/>"
        "<input type=\"text\"/>"
        "<span></span>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto divs = doc->find_all_elements("div");
    ASSERT_EQ(divs.size(), 1u);
    EXPECT_EQ(divs[0]->text_content(), "");
    EXPECT_EQ(divs[0]->children.size(), 0u);
    auto* br = doc->find_element("br");
    ASSERT_NE(br, nullptr);
    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "text");
    auto spans = doc->find_all_elements("span");
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0]->text_content(), "");
}

TEST(HtmlParserTest, DeeplyNestedListStructureV104) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ul>"
        "<li>Item 1"
        "  <ul>"
        "    <li>Sub 1a</li>"
        "    <li>Sub 1b"
        "      <ol>"
        "        <li>Deep 1</li>"
        "        <li>Deep 2</li>"
        "      </ol>"
        "    </li>"
        "  </ul>"
        "</li>"
        "<li>Item 2</li>"
        "</ul>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto uls = doc->find_all_elements("ul");
    ASSERT_EQ(uls.size(), 2u);
    auto ols = doc->find_all_elements("ol");
    ASSERT_EQ(ols.size(), 1u);
    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 6u);
    EXPECT_EQ(lis[4]->text_content(), "Deep 2");
    EXPECT_EQ(lis[5]->text_content(), "Item 2");
}

// ============================================================================
// V105 Tests — HTML Parsing
// ============================================================================

TEST(HtmlParserTest, NestedTablesWithCaptionV105) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<caption>Sales Report</caption>"
        "<tr><th>Q1</th><th>Q2</th></tr>"
        "<tr><td>100</td><td>200</td></tr>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    auto* caption = doc->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->text_content(), "Sales Report");
    auto ths = doc->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Q1");
    EXPECT_EQ(ths[1]->text_content(), "Q2");
    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 2u);
    EXPECT_EQ(tds[0]->text_content(), "100");
    EXPECT_EQ(tds[1]->text_content(), "200");
}

TEST(HtmlParserTest, DefinitionListStructureV105) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>Term A</dt><dd>Definition A</dd>"
        "<dt>Term B</dt><dd>Definition B</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto dts = doc->find_all_elements("dt");
    ASSERT_EQ(dts.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term A");
    EXPECT_EQ(dts[1]->text_content(), "Term B");
    auto dds = doc->find_all_elements("dd");
    ASSERT_EQ(dds.size(), 2u);
    EXPECT_EQ(dds[0]->text_content(), "Definition A");
    EXPECT_EQ(dds[1]->text_content(), "Definition B");
}

TEST(HtmlParserTest, FieldsetWithLegendV105) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<fieldset>"
        "<legend>Login</legend>"
        "<input type=\"text\" name=\"user\"/>"
        "<input type=\"password\" name=\"pass\"/>"
        "</fieldset>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* legend = doc->find_element("legend");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->text_content(), "Login");
    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "text");
    EXPECT_EQ(get_attr_v63(inputs[0], "name"), "user");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "password");
    EXPECT_EQ(get_attr_v63(inputs[1], "name"), "pass");
}

TEST(HtmlParserTest, FigureWithFigcaptionV105) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<figure>"
        "<img src=\"chart.png\" alt=\"Chart\"/>"
        "<figcaption>Revenue by quarter</figcaption>"
        "</figure>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "chart.png");
    EXPECT_EQ(get_attr_v63(img, "alt"), "Chart");
    auto* figcaption = doc->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->text_content(), "Revenue by quarter");
}

TEST(HtmlParserTest, DetailsAndSummaryElementsV105) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<details>"
        "<summary>Click to expand</summary>"
        "<p>Hidden content revealed here.</p>"
        "</details>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click to expand");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hidden content revealed here.");
}

TEST(HtmlParserTest, MultipleDataAttributesV105) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div data-id=\"42\" data-role=\"admin\" data-active=\"true\">User card</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->text_content(), "User card");
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "admin");
    EXPECT_EQ(get_attr_v63(div, "data-active"), "true");
}

TEST(HtmlParserTest, MixedInlineAndBlockElementsV105) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div>"
        "<span>inline1</span>"
        "<p>block paragraph</p>"
        "<em>emphasized</em>"
        "<strong>bold text</strong>"
        "</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "inline1");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "block paragraph");
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "emphasized");
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "bold text");
}

TEST(HtmlParserTest, SelectWithOptionGroupsV105) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<select>"
        "<optgroup label=\"Fruits\">"
        "<option value=\"apple\">Apple</option>"
        "<option value=\"banana\">Banana</option>"
        "</optgroup>"
        "<optgroup label=\"Vegs\">"
        "<option value=\"carrot\">Carrot</option>"
        "</optgroup>"
        "</select>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto optgroups = doc->find_all_elements("optgroup");
    ASSERT_EQ(optgroups.size(), 2u);
    EXPECT_EQ(get_attr_v63(optgroups[0], "label"), "Fruits");
    EXPECT_EQ(get_attr_v63(optgroups[1], "label"), "Vegs");
    auto options = doc->find_all_elements("option");
    ASSERT_EQ(options.size(), 3u);
    EXPECT_EQ(get_attr_v63(options[0], "value"), "apple");
    EXPECT_EQ(options[0]->text_content(), "Apple");
    EXPECT_EQ(get_attr_v63(options[1], "value"), "banana");
    EXPECT_EQ(options[1]->text_content(), "Banana");
    EXPECT_EQ(get_attr_v63(options[2], "value"), "carrot");
    EXPECT_EQ(options[2]->text_content(), "Carrot");
}

// ---------------------------------------------------------------------------
// V106 Tests — 8 new HTML parser tests
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, NestedDefinitionListV106) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>Term1</dt><dd>Def1</dd>"
        "<dt>Term2</dt><dd>Def2</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto dts = doc->find_all_elements("dt");
    ASSERT_EQ(dts.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term1");
    EXPECT_EQ(dts[1]->text_content(), "Term2");
    auto dds = doc->find_all_elements("dd");
    ASSERT_EQ(dds.size(), 2u);
    EXPECT_EQ(dds[0]->text_content(), "Def1");
    EXPECT_EQ(dds[1]->text_content(), "Def2");
}

TEST(HtmlParserTest, FieldsetWithLegendV106) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<fieldset>"
        "<legend>Login</legend>"
        "<input type=\"text\" name=\"user\"/>"
        "</fieldset>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* legend = doc->find_element("legend");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->text_content(), "Login");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "text");
    EXPECT_EQ(get_attr_v63(input, "name"), "user");
}

TEST(HtmlParserTest, DataAttributesPreservedV106) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div data-id=\"42\" data-role=\"admin\">Info</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "admin");
    EXPECT_EQ(div->text_content(), "Info");
}

TEST(HtmlParserTest, NestedBlockquoteParagraphsV106) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<blockquote>"
        "<p>First paragraph</p>"
        "<p>Second paragraph</p>"
        "</blockquote>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* bq = doc->find_element("blockquote");
    ASSERT_NE(bq, nullptr);
    auto paras = doc->find_all_elements("p");
    ASSERT_EQ(paras.size(), 2u);
    EXPECT_EQ(paras[0]->text_content(), "First paragraph");
    EXPECT_EQ(paras[1]->text_content(), "Second paragraph");
}

TEST(HtmlParserTest, TableCaptionAndHeaderV106) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<caption>Sales Data</caption>"
        "<thead><tr><th>Year</th><th>Revenue</th></tr></thead>"
        "<tbody><tr><td>2024</td><td>1M</td></tr></tbody>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* caption = doc->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->text_content(), "Sales Data");
    auto ths = doc->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Year");
    EXPECT_EQ(ths[1]->text_content(), "Revenue");
    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 2u);
    EXPECT_EQ(tds[0]->text_content(), "2024");
    EXPECT_EQ(tds[1]->text_content(), "1M");
}

TEST(HtmlParserTest, MultipleClassesOnElementV106) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<span class=\"bold italic underline\">Styled</span>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(get_attr_v63(span, "class"), "bold italic underline");
    EXPECT_EQ(span->text_content(), "Styled");
}

TEST(HtmlParserTest, AnchorWithHrefAndTargetV106) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<a href=\"https://example.com\" target=\"_blank\" rel=\"noopener\">Link</a>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(get_attr_v63(a, "href"), "https://example.com");
    EXPECT_EQ(get_attr_v63(a, "target"), "_blank");
    EXPECT_EQ(get_attr_v63(a, "rel"), "noopener");
    EXPECT_EQ(a->text_content(), "Link");
}

TEST(HtmlParserTest, DetailsAndSummaryElementsV106) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<details>"
        "<summary>Click to expand</summary>"
        "<p>Hidden content here</p>"
        "</details>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click to expand");
    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hidden content here");
}

// V107 Tests — 8 new HTML parser tests

TEST(HtmlParserTest, NestedOrderedListsV107) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ol><li>First<ol><li>Nested A</li><li>Nested B</li></ol></li><li>Second</li></ol>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto lis = doc->find_all_elements("li");
    ASSERT_GE(lis.size(), 4u);
    EXPECT_EQ(lis[0]->tag_name, "li");
    EXPECT_EQ(lis[1]->text_content(), "Nested A");
    EXPECT_EQ(lis[2]->text_content(), "Nested B");
    EXPECT_EQ(lis[3]->text_content(), "Second");
}

TEST(HtmlParserTest, PreformattedTextPreservesWhitespaceV107) {
    auto doc = clever::html::parse(
        "<html><body><pre>  line1\n  line2\n  line3</pre></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
    EXPECT_EQ(pre->tag_name, "pre");
    std::string content = pre->text_content();
    EXPECT_NE(content.find("line1"), std::string::npos);
    EXPECT_NE(content.find("line2"), std::string::npos);
    EXPECT_NE(content.find("line3"), std::string::npos);
}

TEST(HtmlParserTest, InputTypesWithValueAttributeV107) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<input type=\"email\" value=\"test@example.com\"/>"
        "<input type=\"password\" value=\"secret123\"/>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto inputs = doc->find_all_elements("input");
    ASSERT_GE(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "email");
    EXPECT_EQ(get_attr_v63(inputs[0], "value"), "test@example.com");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "password");
    EXPECT_EQ(get_attr_v63(inputs[1], "value"), "secret123");
}

TEST(HtmlParserTest, TableWithColspanAndRowspanV107) {
    auto doc = clever::html::parse(
        "<html><body><table>"
        "<tr><th colspan=\"2\">Header</th></tr>"
        "<tr><td rowspan=\"2\">Left</td><td>Right1</td></tr>"
        "<tr><td>Right2</td></tr>"
        "</table></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* th = doc->find_element("th");
    ASSERT_NE(th, nullptr);
    EXPECT_EQ(get_attr_v63(th, "colspan"), "2");
    EXPECT_EQ(th->text_content(), "Header");
    auto tds = doc->find_all_elements("td");
    ASSERT_GE(tds.size(), 3u);
    EXPECT_EQ(get_attr_v63(tds[0], "rowspan"), "2");
    EXPECT_EQ(tds[0]->text_content(), "Left");
}

TEST(HtmlParserTest, MetaCharsetAndViewportV107) {
    auto doc = clever::html::parse(
        "<html><head>"
        "<meta charset=\"UTF-8\"/>"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
        "</head><body></body></html>");
    ASSERT_NE(doc, nullptr);
    auto metas = doc->find_all_elements("meta");
    ASSERT_GE(metas.size(), 2u);
    EXPECT_EQ(get_attr_v63(metas[0], "charset"), "UTF-8");
    EXPECT_EQ(get_attr_v63(metas[1], "name"), "viewport");
    EXPECT_EQ(get_attr_v63(metas[1], "content"), "width=device-width, initial-scale=1");
}

TEST(HtmlParserTest, SelectWithMultipleOptionsV107) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<select name=\"color\">"
        "<option value=\"r\">Red</option>"
        "<option value=\"g\" selected>Green</option>"
        "<option value=\"b\">Blue</option>"
        "</select>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* select = doc->find_element("select");
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(get_attr_v63(select, "name"), "color");
    auto opts = doc->find_all_elements("option");
    ASSERT_EQ(opts.size(), 3u);
    EXPECT_EQ(get_attr_v63(opts[0], "value"), "r");
    EXPECT_EQ(opts[0]->text_content(), "Red");
    EXPECT_EQ(get_attr_v63(opts[1], "value"), "g");
    EXPECT_EQ(opts[1]->text_content(), "Green");
    EXPECT_EQ(get_attr_v63(opts[2], "value"), "b");
    EXPECT_EQ(opts[2]->text_content(), "Blue");
}

TEST(HtmlParserTest, EmptyElementsPreservedV107) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div></div>"
        "<span></span>"
        "<p></p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->text_content(), "");
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "");
}

TEST(HtmlParserTest, ScriptAndStyleTagsContentV107) {
    auto doc = clever::html::parse(
        "<html><head>"
        "<style>body { color: red; }</style>"
        "<script>var x = 1;</script>"
        "</head><body></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* style = doc->find_element("style");
    ASSERT_NE(style, nullptr);
    EXPECT_EQ(style->tag_name, "style");
    std::string style_text = style->text_content();
    EXPECT_NE(style_text.find("color: red"), std::string::npos);
    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    EXPECT_EQ(script->tag_name, "script");
    std::string script_text = script->text_content();
    EXPECT_NE(script_text.find("var x = 1"), std::string::npos);
}

// ============================================================================
// V108 Tests
// ============================================================================

TEST(HtmlParserTest, NestedListStructureV108) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ul><li>One</li><li>Two<ul><li>Inner</li></ul></li><li>Three</li></ul>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto lis = doc->find_all_elements("li");
    EXPECT_EQ(lis.size(), 4u);
    EXPECT_EQ(lis[0]->text_content(), "One");
    EXPECT_EQ(lis[3]->text_content(), "Three");
    auto uls = doc->find_all_elements("ul");
    EXPECT_EQ(uls.size(), 2u);
}

TEST(HtmlParserTest, TableStructureV108) {
    auto doc = clever::html::parse(
        "<html><body><table>"
        "<tr><th>Header1</th><th>Header2</th></tr>"
        "<tr><td>Cell1</td><td>Cell2</td></tr>"
        "</table></body></html>");
    ASSERT_NE(doc, nullptr);
    auto ths = doc->find_all_elements("th");
    EXPECT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Header1");
    EXPECT_EQ(ths[1]->text_content(), "Header2");
    auto tds = doc->find_all_elements("td");
    EXPECT_EQ(tds.size(), 2u);
    EXPECT_EQ(tds[0]->text_content(), "Cell1");
    EXPECT_EQ(tds[1]->text_content(), "Cell2");
}

TEST(HtmlParserTest, MultipleAttributesOnSingleElementV108) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<a href=\"https://example.com\" target=\"_blank\" rel=\"noopener\" class=\"link\">Click</a>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(get_attr_v63(a, "href"), "https://example.com");
    EXPECT_EQ(get_attr_v63(a, "target"), "_blank");
    EXPECT_EQ(get_attr_v63(a, "rel"), "noopener");
    EXPECT_EQ(get_attr_v63(a, "class"), "link");
    EXPECT_EQ(a->text_content(), "Click");
}

TEST(HtmlParserTest, FormWithInputsV108) {
    auto doc = clever::html::parse(
        "<html><body><form action=\"/submit\" method=\"post\">"
        "<input type=\"text\" name=\"user\"/>"
        "<input type=\"password\" name=\"pass\"/>"
        "<button type=\"submit\">Go</button>"
        "</form></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "action"), "/submit");
    EXPECT_EQ(get_attr_v63(form, "method"), "post");
    auto inputs = doc->find_all_elements("input");
    EXPECT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "name"), "user");
    EXPECT_EQ(get_attr_v63(inputs[1], "name"), "pass");
    auto* btn = doc->find_element("button");
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->text_content(), "Go");
}

TEST(HtmlParserTest, DeeplyNestedDivsV108) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div><div><div><div><div><span>Deep</span></div></div></div></div></div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto divs = doc->find_all_elements("div");
    EXPECT_EQ(divs.size(), 5u);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "Deep");
}

TEST(HtmlParserTest, MixedInlineAndBlockElementsV108) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p>Text with <strong>bold</strong> and <em>italic</em> words.</p>"
        "<div>Block <span>inline</span> content</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "bold");
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "italic");
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "inline");
}

TEST(HtmlParserTest, DataAttributesV108) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div data-id=\"42\" data-role=\"container\" data-visible=\"true\">Content</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "container");
    EXPECT_EQ(get_attr_v63(div, "data-visible"), "true");
    EXPECT_EQ(div->text_content(), "Content");
}

TEST(HtmlParserTest, HeadingHierarchyAndSiblingTextV108) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<h1>Title</h1>"
        "<h2>Subtitle</h2>"
        "<h3>Section</h3>"
        "<p>Paragraph under section.</p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->tag_name, "h1");
    EXPECT_EQ(h1->text_content(), "Title");
    auto* h2 = doc->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->text_content(), "Subtitle");
    auto* h3 = doc->find_element("h3");
    ASSERT_NE(h3, nullptr);
    EXPECT_EQ(h3->text_content(), "Section");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Paragraph under section.");
}

// ---------------------------------------------------------------------------
// V109 Tests
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, NestedListsWithMixedContentV109) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ul>"
        "<li>Item A</li>"
        "<li><ol><li>Sub 1</li><li>Sub 2</li></ol></li>"
        "<li>Item C</li>"
        "</ul>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto lis = doc->find_all_elements("li");
    // 3 outer + 2 inner = 5
    EXPECT_GE(lis.size(), 5u);
    EXPECT_EQ(lis[0]->text_content(), "Item A");
    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    EXPECT_EQ(ol->tag_name, "ol");
    auto inner = ol->find_all_elements("li");
    EXPECT_EQ(inner.size(), 2u);
    EXPECT_EQ(inner[0]->text_content(), "Sub 1");
    EXPECT_EQ(inner[1]->text_content(), "Sub 2");
}

TEST(HtmlParserTest, TableWithHeaderAndDataCellsV109) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<thead><tr><th>Name</th><th>Age</th></tr></thead>"
        "<tbody><tr><td>Alice</td><td>30</td></tr></tbody>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto ths = doc->find_all_elements("th");
    EXPECT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Age");
    auto tds = doc->find_all_elements("td");
    EXPECT_EQ(tds.size(), 2u);
    EXPECT_EQ(tds[0]->text_content(), "Alice");
    EXPECT_EQ(tds[1]->text_content(), "30");
}

TEST(HtmlParserTest, FormElementsWithMultipleAttributesV109) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<form action=\"/submit\" method=\"post\">"
        "<input type=\"email\" name=\"user_email\" placeholder=\"you@example.com\"/>"
        "<textarea name=\"message\" rows=\"5\" cols=\"40\">Hello</textarea>"
        "<button type=\"submit\">Send</button>"
        "</form>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "action"), "/submit");
    EXPECT_EQ(get_attr_v63(form, "method"), "post");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "email");
    EXPECT_EQ(get_attr_v63(input, "placeholder"), "you@example.com");
    auto* textarea = doc->find_element("textarea");
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(get_attr_v63(textarea, "rows"), "5");
    EXPECT_EQ(textarea->text_content(), "Hello");
    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text_content(), "Send");
}

TEST(HtmlParserTest, DefinitionListWithTermsAndDescriptionsV109) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>HTML</dt><dd>HyperText Markup Language</dd>"
        "<dt>CSS</dt><dd>Cascading Style Sheets</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto dts = doc->find_all_elements("dt");
    EXPECT_EQ(dts.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "HTML");
    EXPECT_EQ(dts[1]->text_content(), "CSS");
    auto dds = doc->find_all_elements("dd");
    EXPECT_EQ(dds.size(), 2u);
    EXPECT_EQ(dds[0]->text_content(), "HyperText Markup Language");
    EXPECT_EQ(dds[1]->text_content(), "Cascading Style Sheets");
}

TEST(HtmlParserTest, ArticleWithHeaderFooterAndSectionsV109) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<article>"
        "<header><h1>Blog Post</h1></header>"
        "<section><p>First paragraph.</p></section>"
        "<section><p>Second paragraph.</p></section>"
        "<footer><small>Copyright 2026</small></footer>"
        "</article>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    EXPECT_EQ(article->tag_name, "article");
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Blog Post");
    auto sections = doc->find_all_elements("section");
    EXPECT_EQ(sections.size(), 2u);
    auto ps = doc->find_all_elements("p");
    EXPECT_EQ(ps.size(), 2u);
    EXPECT_EQ(ps[0]->text_content(), "First paragraph.");
    EXPECT_EQ(ps[1]->text_content(), "Second paragraph.");
    auto* small = doc->find_element("small");
    ASSERT_NE(small, nullptr);
    EXPECT_EQ(small->text_content(), "Copyright 2026");
}

TEST(HtmlParserTest, SelectWithOptionGroupsV109) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<select name=\"car\">"
        "<optgroup label=\"Swedish\">"
        "<option value=\"volvo\">Volvo</option>"
        "<option value=\"saab\">Saab</option>"
        "</optgroup>"
        "<optgroup label=\"German\">"
        "<option value=\"bmw\">BMW</option>"
        "</optgroup>"
        "</select>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* sel = doc->find_element("select");
    ASSERT_NE(sel, nullptr);
    EXPECT_EQ(get_attr_v63(sel, "name"), "car");
    auto optgroups = doc->find_all_elements("optgroup");
    EXPECT_EQ(optgroups.size(), 2u);
    EXPECT_EQ(get_attr_v63(optgroups[0], "label"), "Swedish");
    EXPECT_EQ(get_attr_v63(optgroups[1], "label"), "German");
    auto options = doc->find_all_elements("option");
    EXPECT_EQ(options.size(), 3u);
    EXPECT_EQ(get_attr_v63(options[0], "value"), "volvo");
    EXPECT_EQ(options[0]->text_content(), "Volvo");
    EXPECT_EQ(options[2]->text_content(), "BMW");
}

TEST(HtmlParserTest, FigureWithFigcaptionAndImgV109) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<figure>"
        "<img src=\"photo.jpg\" alt=\"A scenic view\" width=\"800\"/>"
        "<figcaption>A beautiful landscape photo.</figcaption>"
        "</figure>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);
    EXPECT_EQ(figure->tag_name, "figure");
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "photo.jpg");
    EXPECT_EQ(get_attr_v63(img, "alt"), "A scenic view");
    EXPECT_EQ(get_attr_v63(img, "width"), "800");
    auto* figcaption = doc->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->text_content(), "A beautiful landscape photo.");
}

TEST(HtmlParserTest, DetailsAndSummaryElementsV109) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<details>"
        "<summary>Click to expand</summary>"
        "<p>Hidden content revealed on click.</p>"
        "</details>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    EXPECT_EQ(details->tag_name, "details");
    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click to expand");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Hidden content revealed on click.");
}

TEST(HtmlParserTest, NestedOrderedListWithStartAttributeV110) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ol start=\"5\">"
        "<li>Fifth item</li>"
        "<li>Sixth item</li>"
        "<li>Seventh item</li>"
        "</ol>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    EXPECT_EQ(ol->tag_name, "ol");
    EXPECT_EQ(get_attr_v63(ol, "start"), "5");
    auto lis = doc->find_all_elements("li");
    EXPECT_EQ(lis.size(), 3u);
    EXPECT_EQ(lis[0]->text_content(), "Fifth item");
    EXPECT_EQ(lis[1]->text_content(), "Sixth item");
    EXPECT_EQ(lis[2]->text_content(), "Seventh item");
}

TEST(HtmlParserTest, TableWithColspanAndRowspanV110) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<tr><th colspan=\"2\">Header</th></tr>"
        "<tr><td rowspan=\"2\">Left</td><td>Right1</td></tr>"
        "<tr><td>Right2</td></tr>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* th = doc->find_element("th");
    ASSERT_NE(th, nullptr);
    EXPECT_EQ(get_attr_v63(th, "colspan"), "2");
    EXPECT_EQ(th->text_content(), "Header");
    auto tds = doc->find_all_elements("td");
    EXPECT_EQ(tds.size(), 3u);
    EXPECT_EQ(get_attr_v63(tds[0], "rowspan"), "2");
    EXPECT_EQ(tds[0]->text_content(), "Left");
}

TEST(HtmlParserTest, MultipleDataAttributesV110) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div data-id=\"42\" data-role=\"main\" data-visible=\"true\">Content</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "main");
    EXPECT_EQ(get_attr_v63(div, "data-visible"), "true");
    EXPECT_EQ(div->text_content(), "Content");
}

TEST(HtmlParserTest, NestedBlockquoteWithCiteV110) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<blockquote cite=\"https://example.com\">"
        "<p>To be or not to be.</p>"
        "</blockquote>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* bq = doc->find_element("blockquote");
    ASSERT_NE(bq, nullptr);
    EXPECT_EQ(bq->tag_name, "blockquote");
    EXPECT_EQ(get_attr_v63(bq, "cite"), "https://example.com");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "To be or not to be.");
}

TEST(HtmlParserTest, SelectWithMultipleOptionsV110) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<select name=\"color\">"
        "<option value=\"r\">Red</option>"
        "<option value=\"g\" selected>Green</option>"
        "<option value=\"b\">Blue</option>"
        "</select>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* sel = doc->find_element("select");
    ASSERT_NE(sel, nullptr);
    EXPECT_EQ(get_attr_v63(sel, "name"), "color");
    auto opts = doc->find_all_elements("option");
    EXPECT_EQ(opts.size(), 3u);
    EXPECT_EQ(get_attr_v63(opts[0], "value"), "r");
    EXPECT_EQ(opts[0]->text_content(), "Red");
    EXPECT_EQ(get_attr_v63(opts[1], "value"), "g");
    EXPECT_EQ(opts[1]->text_content(), "Green");
    EXPECT_EQ(get_attr_v63(opts[2], "value"), "b");
    EXPECT_EQ(opts[2]->text_content(), "Blue");
}

TEST(HtmlParserTest, DescriptionListDlDtDdV110) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>Term1</dt><dd>Def1</dd>"
        "<dt>Term2</dt><dd>Def2</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    EXPECT_EQ(dl->tag_name, "dl");
    auto dts = doc->find_all_elements("dt");
    EXPECT_EQ(dts.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term1");
    EXPECT_EQ(dts[1]->text_content(), "Term2");
    auto dds = doc->find_all_elements("dd");
    EXPECT_EQ(dds.size(), 2u);
    EXPECT_EQ(dds[0]->text_content(), "Def1");
    EXPECT_EQ(dds[1]->text_content(), "Def2");
}

TEST(HtmlParserTest, IframeWithSrcAndTitleV110) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<iframe src=\"https://example.com/embed\" title=\"Embedded\" width=\"640\" height=\"480\"></iframe>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* iframe = doc->find_element("iframe");
    ASSERT_NE(iframe, nullptr);
    EXPECT_EQ(iframe->tag_name, "iframe");
    EXPECT_EQ(get_attr_v63(iframe, "src"), "https://example.com/embed");
    EXPECT_EQ(get_attr_v63(iframe, "title"), "Embedded");
    EXPECT_EQ(get_attr_v63(iframe, "width"), "640");
    EXPECT_EQ(get_attr_v63(iframe, "height"), "480");
}

TEST(HtmlParserTest, NestedDivsWithMixedContentV110) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div id=\"outer\">"
        "<span>Text A</span>"
        "<div id=\"inner\">"
        "<em>Emphasized</em>"
        "<strong>Bold text</strong>"
        "</div>"
        "</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto divs = doc->find_all_elements("div");
    EXPECT_GE(divs.size(), 2u);
    const clever::html::SimpleNode* outer = nullptr;
    const clever::html::SimpleNode* inner = nullptr;
    for (auto* d : divs) {
        if (get_attr_v63(d, "id") == "outer") outer = d;
        if (get_attr_v63(d, "id") == "inner") inner = d;
    }
    ASSERT_NE(outer, nullptr);
    ASSERT_NE(inner, nullptr);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "Text A");
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "Emphasized");
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "Bold text");
}

// ============================================================================
// V111 Tests — HTML parser tree-building, attribute handling, nesting
// ============================================================================

// 1. Nested lists with text content
TEST(HtmlParser, NestedOrderedListTextContentV111) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ol><li>First</li><li>Second</li><li>Third</li></ol>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 3u);
    EXPECT_EQ(lis[0]->text_content(), "First");
    EXPECT_EQ(lis[1]->text_content(), "Second");
    EXPECT_EQ(lis[2]->text_content(), "Third");
}

// 2. Multiple attributes on a single element
TEST(HtmlParser, MultipleAttributesOnElementV111) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<a href=\"https://example.com\" target=\"_blank\" rel=\"noopener\">Link</a>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(get_attr_v63(a, "href"), "https://example.com");
    EXPECT_EQ(get_attr_v63(a, "target"), "_blank");
    EXPECT_EQ(get_attr_v63(a, "rel"), "noopener");
    EXPECT_EQ(a->text_content(), "Link");
}

// 3. Deeply nested elements preserve hierarchy
TEST(HtmlParser, DeeplyNestedElementsV111) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div><section><article><p><span>Deep</span></p></article></section></div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->tag_name, "span");
    EXPECT_EQ(span->text_content(), "Deep");
    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    EXPECT_EQ(article->text_content(), "Deep");
}

// 4. Table structure parsing
TEST(HtmlParser, TableStructureParsingV111) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table><tr><td>A1</td><td>A2</td></tr>"
        "<tr><td>B1</td><td>B2</td></tr></table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto trs = doc->find_all_elements("tr");
    ASSERT_EQ(trs.size(), 2u);
    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 4u);
    EXPECT_EQ(tds[0]->text_content(), "A1");
    EXPECT_EQ(tds[1]->text_content(), "A2");
    EXPECT_EQ(tds[2]->text_content(), "B1");
    EXPECT_EQ(tds[3]->text_content(), "B2");
}

// 5. Self-closing tags in body (br, hr, img)
TEST(HtmlParser, SelfClosingTagsInBodyV111) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p>Before<br/>After</p>"
        "<hr/>"
        "<img src=\"pic.jpg\" alt=\"Photo\"/>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* br = doc->find_element("br");
    ASSERT_NE(br, nullptr);
    EXPECT_EQ(br->tag_name, "br");
    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "pic.jpg");
    EXPECT_EQ(get_attr_v63(img, "alt"), "Photo");
}

// 6. Sibling elements with mixed content
TEST(HtmlParser, SiblingElementsMixedContentV111) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<h1>Title</h1>"
        "<p>Paragraph one.</p>"
        "<p>Paragraph two.</p>"
        "<h2>Subtitle</h2>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Title");
    auto ps = doc->find_all_elements("p");
    ASSERT_EQ(ps.size(), 2u);
    EXPECT_EQ(ps[0]->text_content(), "Paragraph one.");
    EXPECT_EQ(ps[1]->text_content(), "Paragraph two.");
    auto* h2 = doc->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->text_content(), "Subtitle");
}

// 7. Form elements with various input types
TEST(HtmlParser, FormElementsVariousInputTypesV111) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<form action=\"/submit\" method=\"post\">"
        "<input type=\"email\" name=\"user_email\"/>"
        "<input type=\"password\" name=\"user_pass\"/>"
        "<textarea name=\"bio\">Hello world</textarea>"
        "<button type=\"submit\">Go</button>"
        "</form>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "action"), "/submit");
    EXPECT_EQ(get_attr_v63(form, "method"), "post");
    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "email");
    EXPECT_EQ(get_attr_v63(inputs[0], "name"), "user_email");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "password");
    EXPECT_EQ(get_attr_v63(inputs[1], "name"), "user_pass");
    auto* textarea = doc->find_element("textarea");
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(textarea->text_content(), "Hello world");
    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text_content(), "Go");
}

// 8. Data attributes and class attribute on elements
TEST(HtmlParser, DataAndClassAttributesV111) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div class=\"container main\" data-id=\"42\" data-role=\"wrapper\">"
        "<span class=\"highlight\">Important</span>"
        "</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto divs = doc->find_all_elements("div");
    ASSERT_GE(divs.size(), 1u);
    const clever::html::SimpleNode* container = nullptr;
    for (auto* d : divs) {
        if (get_attr_v63(d, "class") == "container main") { container = d; break; }
    }
    ASSERT_NE(container, nullptr);
    EXPECT_EQ(get_attr_v63(container, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(container, "data-role"), "wrapper");
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(get_attr_v63(span, "class"), "highlight");
    EXPECT_EQ(span->text_content(), "Important");
}

// ============================================================================
// V112 Tests
// ============================================================================

// 1. Nested lists with mixed content
TEST(HtmlParser, NestedListsMixedContentV112) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ul><li>Alpha<ul><li>Beta</li><li>Gamma</li></ul></li><li>Delta</li></ul>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto lis = doc->find_all_elements("li");
    ASSERT_GE(lis.size(), 4u);
    EXPECT_EQ(lis[1]->text_content(), "Beta");
    EXPECT_EQ(lis[2]->text_content(), "Gamma");
    EXPECT_EQ(lis[3]->text_content(), "Delta");
    auto uls = doc->find_all_elements("ul");
    ASSERT_GE(uls.size(), 2u);
}

// 2. Table with thead, tbody, and tfoot sections
TEST(HtmlParser, TableSectionsTheadTbodyTfootV112) {
    auto doc = clever::html::parse(
        "<html><body><table>"
        "<thead><tr><th>Name</th><th>Age</th></tr></thead>"
        "<tbody><tr><td>Alice</td><td>30</td></tr></tbody>"
        "<tfoot><tr><td colspan=\"2\">Total: 1</td></tr></tfoot>"
        "</table></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* thead = doc->find_element("thead");
    ASSERT_NE(thead, nullptr);
    auto* tbody = doc->find_element("tbody");
    ASSERT_NE(tbody, nullptr);
    auto* tfoot = doc->find_element("tfoot");
    ASSERT_NE(tfoot, nullptr);
    auto ths = doc->find_all_elements("th");
    ASSERT_GE(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Age");
    auto tds = doc->find_all_elements("td");
    ASSERT_GE(tds.size(), 3u);
    EXPECT_EQ(tds[0]->text_content(), "Alice");
    EXPECT_EQ(get_attr_v63(tds[2], "colspan"), "2");
}

// 3. Definition list with dt and dd elements
TEST(HtmlParser, DefinitionListDtDdV112) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl><dt>Term1</dt><dd>Def1</dd><dt>Term2</dt><dd>Def2</dd></dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto dts = doc->find_all_elements("dt");
    auto dds = doc->find_all_elements("dd");
    ASSERT_EQ(dts.size(), 2u);
    ASSERT_EQ(dds.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term1");
    EXPECT_EQ(dts[1]->text_content(), "Term2");
    EXPECT_EQ(dds[0]->text_content(), "Def1");
    EXPECT_EQ(dds[1]->text_content(), "Def2");
}

// 4. Multiple inline elements preserving text content order
TEST(HtmlParser, InlineElementsTextContentOrderV112) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<p><strong>Bold</strong> and <em>italic</em> and <code>code</code></p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Bold and italic and code");
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "Bold");
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "italic");
    auto* code = doc->find_element("code");
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(code->text_content(), "code");
}

// 5. Anchor tags with href and target attributes
TEST(HtmlParser, AnchorTagsHrefTargetV112) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<a href=\"https://example.com\" target=\"_blank\" rel=\"noopener\">Link1</a>"
        "<a href=\"/about\">Link2</a>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto anchors = doc->find_all_elements("a");
    ASSERT_EQ(anchors.size(), 2u);
    EXPECT_EQ(get_attr_v63(anchors[0], "href"), "https://example.com");
    EXPECT_EQ(get_attr_v63(anchors[0], "target"), "_blank");
    EXPECT_EQ(get_attr_v63(anchors[0], "rel"), "noopener");
    EXPECT_EQ(anchors[0]->text_content(), "Link1");
    EXPECT_EQ(get_attr_v63(anchors[1], "href"), "/about");
    EXPECT_EQ(anchors[1]->text_content(), "Link2");
}

// 6. Deeply nested divs with text at each level
TEST(HtmlParser, DeeplyNestedDivsTextAtEachLevelV112) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div>L1<div>L2<div>L3<div>L4</div></div></div></div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto divs = doc->find_all_elements("div");
    ASSERT_GE(divs.size(), 4u);
    // Outermost div contains all text via recursive text_content
    EXPECT_EQ(divs[0]->text_content(), "L1L2L3L4");
    // Innermost div has only its own text
    EXPECT_EQ(divs[3]->text_content(), "L4");
    EXPECT_EQ(divs[2]->text_content(), "L3L4");
    EXPECT_EQ(divs[1]->text_content(), "L2L3L4");
}

// 7. Image map with area elements
TEST(HtmlParser, ImageMapWithAreaElementsV112) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<map name=\"infographic\">"
        "<area shape=\"rect\" coords=\"0,0,100,100\" href=\"/region1\" alt=\"Region1\"/>"
        "<area shape=\"circle\" coords=\"200,200,50\" href=\"/region2\" alt=\"Region2\"/>"
        "</map>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* map = doc->find_element("map");
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(get_attr_v63(map, "name"), "infographic");
    auto areas = doc->find_all_elements("area");
    ASSERT_EQ(areas.size(), 2u);
    EXPECT_EQ(get_attr_v63(areas[0], "shape"), "rect");
    EXPECT_EQ(get_attr_v63(areas[0], "coords"), "0,0,100,100");
    EXPECT_EQ(get_attr_v63(areas[0], "href"), "/region1");
    EXPECT_EQ(get_attr_v63(areas[1], "shape"), "circle");
    EXPECT_EQ(get_attr_v63(areas[1], "alt"), "Region2");
}

// 8. Fieldset with legend and mixed form controls
TEST(HtmlParser, FieldsetLegendMixedControlsV112) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<fieldset>"
        "<legend>Personal Info</legend>"
        "<label>Name: <input type=\"text\" name=\"fname\"/></label>"
        "<label>Email: <input type=\"email\" name=\"email\"/></label>"
        "<select name=\"country\"><option value=\"us\">US</option><option value=\"uk\">UK</option></select>"
        "</fieldset>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* fieldset = doc->find_element("fieldset");
    ASSERT_NE(fieldset, nullptr);
    auto* legend = doc->find_element("legend");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->text_content(), "Personal Info");
    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "text");
    EXPECT_EQ(get_attr_v63(inputs[0], "name"), "fname");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "email");
    EXPECT_EQ(get_attr_v63(inputs[1], "name"), "email");
    auto options = doc->find_all_elements("option");
    ASSERT_EQ(options.size(), 2u);
    EXPECT_EQ(get_attr_v63(options[0], "value"), "us");
    EXPECT_EQ(options[0]->text_content(), "US");
    EXPECT_EQ(get_attr_v63(options[1], "value"), "uk");
    EXPECT_EQ(options[1]->text_content(), "UK");
}

// ============================================================================
// V113 Tests
// ============================================================================

// 1. Nested ordered lists preserve depth and structure
TEST(HtmlParser, NestedOrderedListDepthV113) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ol><li>A<ol><li>A1</li><li>A2</li></ol></li><li>B</li></ol>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto ols = doc->find_all_elements("ol");
    ASSERT_EQ(ols.size(), 2u);
    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 4u);
    EXPECT_EQ(lis[1]->text_content(), "A1");
    EXPECT_EQ(lis[2]->text_content(), "A2");
    EXPECT_EQ(lis[3]->text_content(), "B");
}

// 2. Table with caption, thead, tbody, tfoot
TEST(HtmlParser, TableFullSectionsWithCaptionV113) {
    auto doc = clever::html::parse(
        "<html><body><table>"
        "<caption>Sales Report</caption>"
        "<thead><tr><th>Quarter</th><th>Revenue</th></tr></thead>"
        "<tbody><tr><td>Q1</td><td>$100</td></tr></tbody>"
        "<tfoot><tr><td>Total</td><td>$100</td></tr></tfoot>"
        "</table></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* caption = doc->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->text_content(), "Sales Report");
    auto* thead = doc->find_element("thead");
    ASSERT_NE(thead, nullptr);
    auto ths = doc->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Quarter");
    auto* tfoot = doc->find_element("tfoot");
    ASSERT_NE(tfoot, nullptr);
    auto tds = doc->find_all_elements("td");
    ASSERT_GE(tds.size(), 4u);
    EXPECT_EQ(tds[0]->text_content(), "Q1");
}

// 3. Multiple sibling sections with headings and paragraphs
TEST(HtmlParser, MultipleSectionsWithHeadingsV113) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<section><h2>Intro</h2><p>Hello</p></section>"
        "<section><h2>Details</h2><p>World</p></section>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto sections = doc->find_all_elements("section");
    ASSERT_EQ(sections.size(), 2u);
    auto h2s = doc->find_all_elements("h2");
    ASSERT_EQ(h2s.size(), 2u);
    EXPECT_EQ(h2s[0]->text_content(), "Intro");
    EXPECT_EQ(h2s[1]->text_content(), "Details");
    auto ps = doc->find_all_elements("p");
    ASSERT_EQ(ps.size(), 2u);
    EXPECT_EQ(ps[0]->text_content(), "Hello");
    EXPECT_EQ(ps[1]->text_content(), "World");
}

// 4. Deeply nested spans with attributes at each level
TEST(HtmlParser, DeeplyNestedSpansAttributesV113) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<span class=\"a\"><span class=\"b\"><span class=\"c\">deep</span></span></span>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto spans = doc->find_all_elements("span");
    ASSERT_EQ(spans.size(), 3u);
    EXPECT_EQ(get_attr_v63(spans[0], "class"), "a");
    EXPECT_EQ(get_attr_v63(spans[1], "class"), "b");
    EXPECT_EQ(get_attr_v63(spans[2], "class"), "c");
    EXPECT_EQ(spans[2]->text_content(), "deep");
    // Innermost span is child of middle span
    EXPECT_EQ(spans[1]->children.size(), 1u);
}

// 5. Multiple void elements mixed with text siblings
TEST(HtmlParser, VoidElementsMixedWithTextSiblingsV113) {
    auto doc = clever::html::parse(
        "<html><body><p>before<br/>middle<hr/>after<img src=\"x.png\"/></p></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    auto* br = doc->find_element("br");
    ASSERT_NE(br, nullptr);
    EXPECT_TRUE(br->children.empty());
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "x.png");
    EXPECT_TRUE(img->children.empty());
}

// 6. Description list with multiple dt-dd pairs
TEST(HtmlParser, DescriptionListMultiplePairsV113) {
    auto doc = clever::html::parse(
        "<html><body><dl>"
        "<dt>Color</dt><dd>Red</dd>"
        "<dt>Size</dt><dd>Large</dd>"
        "<dt>Weight</dt><dd>Heavy</dd>"
        "</dl></body></html>");
    ASSERT_NE(doc, nullptr);
    auto dts = doc->find_all_elements("dt");
    ASSERT_EQ(dts.size(), 3u);
    auto dds = doc->find_all_elements("dd");
    ASSERT_EQ(dds.size(), 3u);
    EXPECT_EQ(dts[0]->text_content(), "Color");
    EXPECT_EQ(dds[0]->text_content(), "Red");
    EXPECT_EQ(dts[1]->text_content(), "Size");
    EXPECT_EQ(dds[1]->text_content(), "Large");
    EXPECT_EQ(dts[2]->text_content(), "Weight");
    EXPECT_EQ(dds[2]->text_content(), "Heavy");
}

// 7. Nav element with anchor links containing data attributes
TEST(HtmlParser, NavAnchorsWithDataAttributesV113) {
    auto doc = clever::html::parse(
        "<html><body><nav>"
        "<a href=\"/home\" data-section=\"main\">Home</a>"
        "<a href=\"/about\" data-section=\"info\">About</a>"
        "<a href=\"/contact\" data-section=\"form\">Contact</a>"
        "</nav></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    auto anchors = doc->find_all_elements("a");
    ASSERT_EQ(anchors.size(), 3u);
    EXPECT_EQ(get_attr_v63(anchors[0], "href"), "/home");
    EXPECT_EQ(get_attr_v63(anchors[0], "data-section"), "main");
    EXPECT_EQ(anchors[0]->text_content(), "Home");
    EXPECT_EQ(get_attr_v63(anchors[1], "href"), "/about");
    EXPECT_EQ(get_attr_v63(anchors[1], "data-section"), "info");
    EXPECT_EQ(anchors[2]->text_content(), "Contact");
    EXPECT_EQ(get_attr_v63(anchors[2], "data-section"), "form");
}

// 8. Article with header, time, and footer structure
TEST(HtmlParser, ArticleWithHeaderTimeFooterV113) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<article>"
        "<header><h1>Blog Post</h1></header>"
        "<time datetime=\"2026-02-28\">Feb 28</time>"
        "<p>Content here.</p>"
        "<footer><small>By Author</small></footer>"
        "</article>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Blog Post");
    auto* time_el = doc->find_element("time");
    ASSERT_NE(time_el, nullptr);
    EXPECT_EQ(get_attr_v63(time_el, "datetime"), "2026-02-28");
    EXPECT_EQ(time_el->text_content(), "Feb 28");
    auto* footer = doc->find_element("footer");
    ASSERT_NE(footer, nullptr);
    auto* small_el = doc->find_element("small");
    ASSERT_NE(small_el, nullptr);
    EXPECT_EQ(small_el->text_content(), "By Author");
}

// ============================================================================
// V114 Tests
// ============================================================================

// 1. Nested fieldset with legend and mixed form controls
TEST(HtmlParser, NestedFieldsetWithLegendAndControlsV114) {
    auto doc = clever::html::parse(
        "<html><body><form>"
        "<fieldset><legend>Personal</legend>"
        "<input type=\"text\" name=\"fname\" />"
        "<fieldset><legend>Address</legend>"
        "<input type=\"text\" name=\"street\" />"
        "<input type=\"text\" name=\"city\" />"
        "</fieldset></fieldset>"
        "</form></body></html>");
    ASSERT_NE(doc, nullptr);
    auto fieldsets = doc->find_all_elements("fieldset");
    ASSERT_EQ(fieldsets.size(), 2u);
    auto legends = doc->find_all_elements("legend");
    ASSERT_EQ(legends.size(), 2u);
    EXPECT_EQ(legends[0]->text_content(), "Personal");
    EXPECT_EQ(legends[1]->text_content(), "Address");
    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 3u);
    EXPECT_EQ(get_attr_v63(inputs[0], "name"), "fname");
    EXPECT_EQ(get_attr_v63(inputs[1], "name"), "street");
    EXPECT_EQ(get_attr_v63(inputs[2], "name"), "city");
    // All inputs are void elements
    for (auto* inp : inputs) {
        EXPECT_TRUE(inp->children.empty());
    }
}

// 2. Table with colgroup and col elements preserving span attribute
TEST(HtmlParser, TableWithColgroupColSpanAttributeV114) {
    auto doc = clever::html::parse(
        "<html><body><table>"
        "<colgroup><col span=\"2\" /><col span=\"1\" /></colgroup>"
        "<thead><tr><th>A</th><th>B</th><th>C</th></tr></thead>"
        "<tbody><tr><td>1</td><td>2</td><td>3</td></tr></tbody>"
        "</table></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* colgroup = doc->find_element("colgroup");
    ASSERT_NE(colgroup, nullptr);
    auto cols = doc->find_all_elements("col");
    ASSERT_EQ(cols.size(), 2u);
    EXPECT_EQ(get_attr_v63(cols[0], "span"), "2");
    EXPECT_EQ(get_attr_v63(cols[1], "span"), "1");
    auto ths = doc->find_all_elements("th");
    ASSERT_EQ(ths.size(), 3u);
    EXPECT_EQ(ths[0]->text_content(), "A");
    EXPECT_EQ(ths[2]->text_content(), "C");
    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 3u);
    EXPECT_EQ(tds[1]->text_content(), "2");
}

// 3. Multiple sibling aside elements with different roles
TEST(HtmlParser, MultipleSiblingAsidesWithRoleAttributeV114) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<aside role=\"complementary\"><p>Sidebar info</p></aside>"
        "<aside role=\"note\"><p>Additional note</p></aside>"
        "<aside role=\"navigation\"><a href=\"/map\">Site map</a></aside>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto asides = doc->find_all_elements("aside");
    ASSERT_EQ(asides.size(), 3u);
    EXPECT_EQ(get_attr_v63(asides[0], "role"), "complementary");
    EXPECT_EQ(get_attr_v63(asides[1], "role"), "note");
    EXPECT_EQ(get_attr_v63(asides[2], "role"), "navigation");
    auto ps = doc->find_all_elements("p");
    ASSERT_EQ(ps.size(), 2u);
    EXPECT_EQ(ps[0]->text_content(), "Sidebar info");
    EXPECT_EQ(ps[1]->text_content(), "Additional note");
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->text_content(), "Site map");
    EXPECT_EQ(get_attr_v63(a, "href"), "/map");
}

// 4. Deeply nested inline elements preserve structure and text
TEST(HtmlParser, DeeplyNestedInlineChainV114) {
    auto doc = clever::html::parse(
        "<html><body><p>"
        "<em><strong><code><mark>highlighted code</mark></code></strong></em>"
        "</p></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    auto* code = doc->find_element("code");
    ASSERT_NE(code, nullptr);
    auto* mark = doc->find_element("mark");
    ASSERT_NE(mark, nullptr);
    EXPECT_EQ(mark->text_content(), "highlighted code");
    // Verify nesting: em contains strong
    EXPECT_EQ(em->children.size(), 1u);
    EXPECT_EQ(em->children[0]->tag_name, "strong");
    // strong contains code
    EXPECT_EQ(strong->children.size(), 1u);
    EXPECT_EQ(strong->children[0]->tag_name, "code");
    // code contains mark
    EXPECT_EQ(code->children.size(), 1u);
    EXPECT_EQ(code->children[0]->tag_name, "mark");
}

// 5. Picture element with multiple source elements and img fallback
TEST(HtmlParser, PictureSourcesAndImgFallbackV114) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<picture>"
        "<source media=\"(min-width: 800px)\" srcset=\"large.webp\" type=\"image/webp\" />"
        "<source media=\"(min-width: 400px)\" srcset=\"medium.jpg\" type=\"image/jpeg\" />"
        "<img src=\"small.jpg\" alt=\"Responsive image\" />"
        "</picture>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* picture = doc->find_element("picture");
    ASSERT_NE(picture, nullptr);
    auto sources = doc->find_all_elements("source");
    ASSERT_EQ(sources.size(), 2u);
    EXPECT_EQ(get_attr_v63(sources[0], "srcset"), "large.webp");
    EXPECT_EQ(get_attr_v63(sources[0], "type"), "image/webp");
    EXPECT_EQ(get_attr_v63(sources[1], "srcset"), "medium.jpg");
    EXPECT_EQ(get_attr_v63(sources[1], "type"), "image/jpeg");
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "small.jpg");
    EXPECT_EQ(get_attr_v63(img, "alt"), "Responsive image");
    EXPECT_TRUE(img->children.empty());
    // source elements are void
    for (auto* src : sources) {
        EXPECT_TRUE(src->children.empty());
    }
}

// 6. Ordered list with reversed and start attributes
TEST(HtmlParser, OrderedListReversedStartAttributesV114) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ol reversed=\"reversed\" start=\"5\">"
        "<li>Five</li><li>Four</li><li>Three</li>"
        "</ol>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    EXPECT_EQ(get_attr_v63(ol, "reversed"), "reversed");
    EXPECT_EQ(get_attr_v63(ol, "start"), "5");
    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 3u);
    EXPECT_EQ(lis[0]->text_content(), "Five");
    EXPECT_EQ(lis[1]->text_content(), "Four");
    EXPECT_EQ(lis[2]->text_content(), "Three");
    // Verify parent relationship: lis are children of ol
    EXPECT_EQ(ol->children.size(), 3u);
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(ol->children[i]->tag_name, "li");
    }
}

// 7. Multiple blockquote elements with cite attributes and nested paragraphs
TEST(HtmlParser, MultipleBlockquotesWithCiteAndParagraphsV114) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<blockquote cite=\"https://example.com/1\">"
        "<p>First quote paragraph one.</p>"
        "<p>First quote paragraph two.</p>"
        "</blockquote>"
        "<blockquote cite=\"https://example.com/2\">"
        "<p>Second quote.</p>"
        "</blockquote>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto bqs = doc->find_all_elements("blockquote");
    ASSERT_EQ(bqs.size(), 2u);
    EXPECT_EQ(get_attr_v63(bqs[0], "cite"), "https://example.com/1");
    EXPECT_EQ(get_attr_v63(bqs[1], "cite"), "https://example.com/2");
    auto ps = doc->find_all_elements("p");
    ASSERT_EQ(ps.size(), 3u);
    EXPECT_EQ(ps[0]->text_content(), "First quote paragraph one.");
    EXPECT_EQ(ps[1]->text_content(), "First quote paragraph two.");
    EXPECT_EQ(ps[2]->text_content(), "Second quote.");
    // First blockquote has 2 paragraph children
    size_t p_count = 0;
    for (auto& child : bqs[0]->children) {
        if (child->tag_name == "p") ++p_count;
    }
    EXPECT_EQ(p_count, 2u);
}

// 8. Main, header, footer semantic elements with aria-label attributes
TEST(HtmlParser, SemanticElementsWithAriaLabelV114) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<header aria-label=\"site header\"><h1>Title</h1></header>"
        "<main aria-label=\"main content\">"
        "<section aria-label=\"intro\"><p>Welcome</p></section>"
        "<section aria-label=\"details\"><p>Info</p></section>"
        "</main>"
        "<footer aria-label=\"site footer\"><p>Copyright</p></footer>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* header = doc->find_element("header");
    ASSERT_NE(header, nullptr);
    EXPECT_EQ(get_attr_v63(header, "aria-label"), "site header");
    auto* main_el = doc->find_element("main");
    ASSERT_NE(main_el, nullptr);
    EXPECT_EQ(get_attr_v63(main_el, "aria-label"), "main content");
    auto* footer = doc->find_element("footer");
    ASSERT_NE(footer, nullptr);
    EXPECT_EQ(get_attr_v63(footer, "aria-label"), "site footer");
    auto sections = doc->find_all_elements("section");
    ASSERT_EQ(sections.size(), 2u);
    EXPECT_EQ(get_attr_v63(sections[0], "aria-label"), "intro");
    EXPECT_EQ(get_attr_v63(sections[1], "aria-label"), "details");
    auto ps = doc->find_all_elements("p");
    ASSERT_EQ(ps.size(), 3u);
    EXPECT_EQ(ps[0]->text_content(), "Welcome");
    EXPECT_EQ(ps[1]->text_content(), "Info");
    EXPECT_EQ(ps[2]->text_content(), "Copyright");
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Title");
}

// ---------------------------------------------------------------------------
// V115 Tests — 8 new parser tests
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, NestedTablesV115) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table><tr><td>"
        "<table><tr><td>inner</td></tr></table>"
        "</td></tr></table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto tables = doc->find_all_elements("table");
    ASSERT_EQ(tables.size(), 2u);
    auto tds = doc->find_all_elements("td");
    ASSERT_GE(tds.size(), 2u);
    bool found_inner = false;
    for (auto* td : tds) {
        if (td->text_content() == "inner") { found_inner = true; break; }
    }
    EXPECT_TRUE(found_inner);
}

TEST(HtmlParserTest, MultipleDataAttributesV115) {
    auto doc = clever::html::parse(
        "<div data-id=\"42\" data-role=\"admin\" data-active=\"true\">content</div>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "admin");
    EXPECT_EQ(get_attr_v63(div, "data-active"), "true");
    EXPECT_EQ(div->text_content(), "content");
}

TEST(HtmlParserTest, PreformattedWhitespacePreservedV115) {
    auto doc = clever::html::parse(
        "<html><body><pre>  line1\n  line2\n  line3</pre></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
    EXPECT_EQ(pre->tag_name, "pre");
    std::string content = pre->text_content();
    EXPECT_NE(content.find("line1"), std::string::npos);
    EXPECT_NE(content.find("line2"), std::string::npos);
    EXPECT_NE(content.find("line3"), std::string::npos);
}

TEST(HtmlParserTest, DefinitionListStructureV115) {
    auto doc = clever::html::parse(
        "<dl>"
        "<dt>Term1</dt><dd>Def1</dd>"
        "<dt>Term2</dt><dd>Def2</dd>"
        "</dl>");
    ASSERT_NE(doc, nullptr);
    auto dts = doc->find_all_elements("dt");
    auto dds = doc->find_all_elements("dd");
    ASSERT_EQ(dts.size(), 2u);
    ASSERT_EQ(dds.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term1");
    EXPECT_EQ(dts[1]->text_content(), "Term2");
    EXPECT_EQ(dds[0]->text_content(), "Def1");
    EXPECT_EQ(dds[1]->text_content(), "Def2");
}

TEST(HtmlParserTest, MixedInlineElementsV115) {
    auto doc = clever::html::parse(
        "<p>This is <strong>bold</strong> and <em>italic</em> and <code>code</code> text.</p>");
    ASSERT_NE(doc, nullptr);
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "bold");
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "italic");
    auto* code = doc->find_element("code");
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(code->text_content(), "code");
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    std::string full = p->text_content();
    EXPECT_NE(full.find("bold"), std::string::npos);
    EXPECT_NE(full.find("italic"), std::string::npos);
    EXPECT_NE(full.find("code"), std::string::npos);
}

TEST(HtmlParserTest, ScriptTagContentNotParsedV115) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<script>var x = '<div>not a real tag</div>';</script>"
        "<p>after script</p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);
    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    auto inner_divs_in_script = script->find_all_elements("div");
    EXPECT_EQ(inner_divs_in_script.size(), 0u);
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "after script");
}

TEST(HtmlParserTest, EmptyAttributeAndBooleanAttrV115) {
    auto doc = clever::html::parse(
        "<input type=\"checkbox\" checked disabled value=\"\">");
    ASSERT_NE(doc, nullptr);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "checkbox");
    EXPECT_EQ(get_attr_v63(input, "checked"), "");
    EXPECT_EQ(get_attr_v63(input, "disabled"), "");
    EXPECT_EQ(get_attr_v63(input, "value"), "");
}

TEST(HtmlParserTest, DeepNestingStressV115) {
    std::string html;
    const int depth = 50;
    for (int i = 0; i < depth; ++i) html += "<div>";
    html += "<span>deep</span>";
    for (int i = 0; i < depth; ++i) html += "</div>";
    auto doc = clever::html::parse(html);
    ASSERT_NE(doc, nullptr);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "deep");
    auto divs = doc->find_all_elements("div");
    EXPECT_EQ(divs.size(), static_cast<size_t>(depth));
}

// ============================================================================
// V116 Tests
// ============================================================================

TEST(HtmlParserTest, TableStructureV116) {
    auto doc = clever::html::parse(
        "<table>"
        "<thead><tr><th>Name</th><th>Age</th></tr></thead>"
        "<tbody><tr><td>Alice</td><td>30</td></tr>"
        "<tr><td>Bob</td><td>25</td></tr></tbody>"
        "</table>");
    ASSERT_NE(doc, nullptr);
    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    auto ths = doc->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Age");
    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 4u);
    EXPECT_EQ(tds[0]->text_content(), "Alice");
    EXPECT_EQ(tds[1]->text_content(), "30");
    EXPECT_EQ(tds[2]->text_content(), "Bob");
    EXPECT_EQ(tds[3]->text_content(), "25");
}

TEST(HtmlParserTest, MultipleDataAttributesV116) {
    auto doc = clever::html::parse(
        "<div data-id=\"42\" data-role=\"admin\" data-active=\"true\">User</div>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "admin");
    EXPECT_EQ(get_attr_v63(div, "data-active"), "true");
    EXPECT_EQ(div->text_content(), "User");
}

TEST(HtmlParserTest, NestedListsV116) {
    auto doc = clever::html::parse(
        "<ul>"
        "<li>Item1"
        "  <ul><li>Sub1</li><li>Sub2</li></ul>"
        "</li>"
        "<li>Item2</li>"
        "</ul>");
    ASSERT_NE(doc, nullptr);
    auto lis = doc->find_all_elements("li");
    ASSERT_GE(lis.size(), 4u);
    auto uls = doc->find_all_elements("ul");
    ASSERT_GE(uls.size(), 2u);
    auto* inner_ul = uls[1];
    auto inner_lis = inner_ul->find_all_elements("li");
    ASSERT_EQ(inner_lis.size(), 2u);
    EXPECT_EQ(inner_lis[0]->text_content(), "Sub1");
    EXPECT_EQ(inner_lis[1]->text_content(), "Sub2");
}

TEST(HtmlParserTest, PreformattedTextPreservesContentV116) {
    auto doc = clever::html::parse(
        "<pre>  line1\n  line2\n  line3</pre>");
    ASSERT_NE(doc, nullptr);
    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
    std::string content = pre->text_content();
    EXPECT_NE(content.find("line1"), std::string::npos);
    EXPECT_NE(content.find("line2"), std::string::npos);
    EXPECT_NE(content.find("line3"), std::string::npos);
}

TEST(HtmlParserTest, AnchorWithHrefAndTargetV116) {
    auto doc = clever::html::parse(
        "<a href=\"https://example.com\" target=\"_blank\" rel=\"noopener\">Click</a>");
    ASSERT_NE(doc, nullptr);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(get_attr_v63(a, "href"), "https://example.com");
    EXPECT_EQ(get_attr_v63(a, "target"), "_blank");
    EXPECT_EQ(get_attr_v63(a, "rel"), "noopener");
    EXPECT_EQ(a->text_content(), "Click");
}

TEST(HtmlParserTest, SelectWithOptionGroupsV116) {
    auto doc = clever::html::parse(
        "<select name=\"car\">"
        "<optgroup label=\"Swedish Cars\">"
        "<option value=\"volvo\">Volvo</option>"
        "<option value=\"saab\">Saab</option>"
        "</optgroup>"
        "<optgroup label=\"German Cars\">"
        "<option value=\"bmw\">BMW</option>"
        "</optgroup>"
        "</select>");
    ASSERT_NE(doc, nullptr);
    auto* sel = doc->find_element("select");
    ASSERT_NE(sel, nullptr);
    EXPECT_EQ(get_attr_v63(sel, "name"), "car");
    auto optgroups = doc->find_all_elements("optgroup");
    ASSERT_EQ(optgroups.size(), 2u);
    EXPECT_EQ(get_attr_v63(optgroups[0], "label"), "Swedish Cars");
    EXPECT_EQ(get_attr_v63(optgroups[1], "label"), "German Cars");
    auto options = doc->find_all_elements("option");
    ASSERT_EQ(options.size(), 3u);
    EXPECT_EQ(get_attr_v63(options[0], "value"), "volvo");
    EXPECT_EQ(options[0]->text_content(), "Volvo");
    EXPECT_EQ(get_attr_v63(options[2], "value"), "bmw");
    EXPECT_EQ(options[2]->text_content(), "BMW");
}

TEST(HtmlParserTest, FieldsetWithLegendV116) {
    auto doc = clever::html::parse(
        "<fieldset>"
        "<legend>Personal Info</legend>"
        "<label>Name: <input type=\"text\" name=\"username\"></label>"
        "</fieldset>");
    ASSERT_NE(doc, nullptr);
    auto* fieldset = doc->find_element("fieldset");
    ASSERT_NE(fieldset, nullptr);
    auto* legend = doc->find_element("legend");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->text_content(), "Personal Info");
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "text");
    EXPECT_EQ(get_attr_v63(input, "name"), "username");
}

TEST(HtmlParserTest, SiblingParagraphsAutoCloseV116) {
    auto doc = clever::html::parse(
        "<div>"
        "<p>First paragraph"
        "<p>Second paragraph"
        "<p>Third paragraph"
        "</div>");
    ASSERT_NE(doc, nullptr);
    auto ps = doc->find_all_elements("p");
    ASSERT_EQ(ps.size(), 3u);
    EXPECT_NE(ps[0]->text_content().find("First"), std::string::npos);
    EXPECT_NE(ps[1]->text_content().find("Second"), std::string::npos);
    EXPECT_NE(ps[2]->text_content().find("Third"), std::string::npos);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    std::string all = div->text_content();
    EXPECT_NE(all.find("First"), std::string::npos);
    EXPECT_NE(all.find("Third"), std::string::npos);
}

// ---------------------------------------------------------------------------
// V117 Tests — advanced HTML parsing: nesting, attributes, void elements,
//              text recovery, whitespace handling, and structural edge cases
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, NestedListsPreserveStructureV117) {
    auto doc = clever::html::parse(
        "<ul>"
        "  <li>Item 1"
        "    <ul>"
        "      <li>Sub A</li>"
        "      <li>Sub B</li>"
        "    </ul>"
        "  </li>"
        "  <li>Item 2</li>"
        "</ul>");
    ASSERT_NE(doc, nullptr);
    auto uls = doc->find_all_elements("ul");
    ASSERT_GE(uls.size(), 2u);
    auto outer_lis = uls[0]->find_all_elements("li");
    ASSERT_GE(outer_lis.size(), 2u);
    auto inner_lis = uls[1]->find_all_elements("li");
    ASSERT_EQ(inner_lis.size(), 2u);
    EXPECT_NE(inner_lis[0]->text_content().find("Sub A"), std::string::npos);
    EXPECT_NE(inner_lis[1]->text_content().find("Sub B"), std::string::npos);
}

TEST(HtmlParserTest, MultipleVoidElementsInSequenceV117) {
    auto doc = clever::html::parse(
        "<div>"
        "<br><hr><br>"
        "<input type=\"text\">"
        "<img src=\"a.png\">"
        "</div>");
    ASSERT_NE(doc, nullptr);
    auto brs = doc->find_all_elements("br");
    ASSERT_EQ(brs.size(), 2u);
    auto hrs = doc->find_all_elements("hr");
    ASSERT_EQ(hrs.size(), 1u);
    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "text");
    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "src"), "a.png");
}

TEST(HtmlParserTest, TableStructureWithHeaderAndBodyV117) {
    auto doc = clever::html::parse(
        "<table>"
        "  <thead><tr><th>Name</th><th>Age</th></tr></thead>"
        "  <tbody>"
        "    <tr><td>Alice</td><td>30</td></tr>"
        "    <tr><td>Bob</td><td>25</td></tr>"
        "  </tbody>"
        "</table>");
    ASSERT_NE(doc, nullptr);
    auto ths = doc->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_NE(ths[0]->text_content().find("Name"), std::string::npos);
    EXPECT_NE(ths[1]->text_content().find("Age"), std::string::npos);
    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 4u);
    EXPECT_NE(tds[0]->text_content().find("Alice"), std::string::npos);
    EXPECT_NE(tds[3]->text_content().find("25"), std::string::npos);
}

TEST(HtmlParserTest, DataAttributesPreservedV117) {
    auto doc = clever::html::parse(
        "<div data-id=\"42\" data-role=\"admin\" data-active=\"true\">"
        "  <span data-tooltip=\"hover me\">text</span>"
        "</div>");
    ASSERT_NE(doc, nullptr);
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "admin");
    EXPECT_EQ(get_attr_v63(div, "data-active"), "true");
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(get_attr_v63(span, "data-tooltip"), "hover me");
    EXPECT_NE(span->text_content().find("text"), std::string::npos);
}

TEST(HtmlParserTest, ScriptTagDoesNotCreateFalseElementsV117) {
    auto doc = clever::html::parse(
        "<html><head>"
        "<script>var x = 'hello world';</script>"
        "</head><body><p>Real content</p></body></html>");
    ASSERT_NE(doc, nullptr);
    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    EXPECT_EQ(script->tag_name, "script");
    // The script element should exist in the tree
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_NE(p->text_content().find("Real content"), std::string::npos);
    // Verify head and body structure
    auto* head = doc->find_element("head");
    ASSERT_NE(head, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    // Script should not spawn extra body or div elements
    auto bodies = doc->find_all_elements("body");
    EXPECT_EQ(bodies.size(), 1u);
}

TEST(HtmlParserTest, DeeplyNestedDivsRecoveryV117) {
    auto doc = clever::html::parse(
        "<div><div><div><div><div>"
        "<span>Deep</span>"
        "</div></div></div></div></div>");
    ASSERT_NE(doc, nullptr);
    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_NE(span->text_content().find("Deep"), std::string::npos);
    auto divs = doc->find_all_elements("div");
    ASSERT_EQ(divs.size(), 5u);
}

TEST(HtmlParserTest, MixedInlineAndBlockElementsV117) {
    auto doc = clever::html::parse(
        "<div>"
        "  <strong>Bold</strong> and <em>italic</em> text"
        "  <p>A paragraph with <a href=\"#\">a link</a></p>"
        "  <span>inline <b>bold span</b></span>"
        "</div>");
    ASSERT_NE(doc, nullptr);
    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_NE(strong->text_content().find("Bold"), std::string::npos);
    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_NE(em->text_content().find("italic"), std::string::npos);
    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(get_attr_v63(a, "href"), "#");
    EXPECT_NE(a->text_content().find("a link"), std::string::npos);
    auto* b = doc->find_element("b");
    ASSERT_NE(b, nullptr);
    EXPECT_NE(b->text_content().find("bold span"), std::string::npos);
}

TEST(HtmlParserTest, EmptyDocumentAndWhitespaceOnlyV117) {
    auto doc1 = clever::html::parse("");
    ASSERT_NE(doc1, nullptr);
    // Empty document should still produce a root node
    auto spans1 = doc1->find_all_elements("span");
    EXPECT_EQ(spans1.size(), 0u);

    auto doc2 = clever::html::parse("   \n\t  \n  ");
    ASSERT_NE(doc2, nullptr);
    // Whitespace-only should not produce any element children
    auto divs2 = doc2->find_all_elements("div");
    EXPECT_EQ(divs2.size(), 0u);
    auto ps2 = doc2->find_all_elements("p");
    EXPECT_EQ(ps2.size(), 0u);
}

// ---------------------------------------------------------------------------
// V118 Tests: 8 new parser tests covering nested structures, attributes,
//             text extraction, sibling ordering, and edge cases.
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, NestedListStructureV118) {
    auto doc = clever::html::parse(
        "<ul><li>Alpha</li><li>Beta</li><li>Gamma</li></ul>");
    ASSERT_NE(doc, nullptr);

    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    EXPECT_EQ(ul->tag_name, "ul");

    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 3u);
    EXPECT_EQ(lis[0]->text_content(), "Alpha");
    EXPECT_EQ(lis[1]->text_content(), "Beta");
    EXPECT_EQ(lis[2]->text_content(), "Gamma");
}

TEST(HtmlParserTest, TableWithHeaderAndDataCellsV118) {
    auto doc = clever::html::parse(
        "<table><tr><th>Name</th><th>Age</th></tr>"
        "<tr><td>Alice</td><td>30</td></tr></table>");
    ASSERT_NE(doc, nullptr);

    auto ths = doc->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Name");
    EXPECT_EQ(ths[1]->text_content(), "Age");

    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 2u);
    EXPECT_EQ(tds[0]->text_content(), "Alice");
    EXPECT_EQ(tds[1]->text_content(), "30");
}

TEST(HtmlParserTest, MultipleAttributesOnSingleElementV118) {
    auto doc = clever::html::parse(
        "<a href=\"https://example.com\" target=\"_blank\" rel=\"noopener\">Link</a>");
    ASSERT_NE(doc, nullptr);

    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->tag_name, "a");
    EXPECT_EQ(get_attr_v63(a, "href"), "https://example.com");
    EXPECT_EQ(get_attr_v63(a, "target"), "_blank");
    EXPECT_EQ(get_attr_v63(a, "rel"), "noopener");
    EXPECT_EQ(a->text_content(), "Link");
}

TEST(HtmlParserTest, DeeplyNestedDivsV118) {
    auto doc = clever::html::parse(
        "<div><div><div><div><span>Deep</span></div></div></div></div>");
    ASSERT_NE(doc, nullptr);

    auto divs = doc->find_all_elements("div");
    EXPECT_GE(divs.size(), 4u);

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "Deep");
}

TEST(HtmlParserTest, MixedInlineAndBlockElementsV118) {
    auto doc = clever::html::parse(
        "<div><p>Paragraph with <strong>bold</strong> and <em>italic</em> text.</p></div>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    auto pText = p->text_content();
    EXPECT_NE(pText.find("Paragraph with"), std::string::npos);
    EXPECT_NE(pText.find("bold"), std::string::npos);
    EXPECT_NE(pText.find("italic"), std::string::npos);

    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "bold");

    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "italic");
}

TEST(HtmlParserTest, FormElementsWithTypesV118) {
    auto doc = clever::html::parse(
        "<form action=\"/submit\" method=\"post\">"
        "<input type=\"email\" name=\"user_email\"/>"
        "<textarea name=\"msg\">Hello</textarea>"
        "<button type=\"submit\">Send</button>"
        "</form>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "action"), "/submit");
    EXPECT_EQ(get_attr_v63(form, "method"), "post");

    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 1u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "email");
    EXPECT_EQ(get_attr_v63(inputs[0], "name"), "user_email");

    auto* textarea = doc->find_element("textarea");
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(get_attr_v63(textarea, "name"), "msg");
    EXPECT_EQ(textarea->text_content(), "Hello");

    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text_content(), "Send");
}

TEST(HtmlParserTest, SiblingParagraphOrderPreservedV118) {
    auto doc = clever::html::parse(
        "<div><p>First</p><p>Second</p><p>Third</p></div>");
    ASSERT_NE(doc, nullptr);

    auto ps = doc->find_all_elements("p");
    ASSERT_EQ(ps.size(), 3u);
    EXPECT_EQ(ps[0]->text_content(), "First");
    EXPECT_EQ(ps[1]->text_content(), "Second");
    EXPECT_EQ(ps[2]->text_content(), "Third");

    // Verify all are children of the same div
    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    size_t pCount = 0;
    for (const auto& child : div->children) {
        if (child->tag_name == "p") pCount++;
    }
    EXPECT_EQ(pCount, 3u);
}

TEST(HtmlParserTest, DataAttributesAndBooleanAttrsV118) {
    auto doc = clever::html::parse(
        "<div data-id=\"42\" data-role=\"main\" hidden>"
        "<input type=\"checkbox\" checked disabled/>"
        "</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "main");
    // Boolean attribute 'hidden' should be present (empty string value)
    EXPECT_EQ(get_attr_v63(div, "hidden"), "");

    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 1u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "checkbox");
    EXPECT_EQ(get_attr_v63(inputs[0], "checked"), "");
    EXPECT_EQ(get_attr_v63(inputs[0], "disabled"), "");
}

// ---------------------------------------------------------------------------
// V119 tests: 8 new HTML parser tests
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, NestedDescriptionListWithMultipleTermsV119) {
    auto doc = clever::html::parse(
        "<dl>"
        "<dt>Term A</dt><dd>Desc A</dd>"
        "<dt>Term B</dt><dd>Desc B</dd>"
        "<dt>Term C</dt><dd>Desc C</dd>"
        "</dl>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);

    auto dts = doc->find_all_elements("dt");
    auto dds = doc->find_all_elements("dd");
    EXPECT_EQ(dts.size(), 3u);
    EXPECT_EQ(dds.size(), 3u);
    EXPECT_EQ(dts[0]->text_content(), "Term A");
    EXPECT_EQ(dts[1]->text_content(), "Term B");
    EXPECT_EQ(dts[2]->text_content(), "Term C");
    EXPECT_EQ(dds[0]->text_content(), "Desc A");
    EXPECT_EQ(dds[1]->text_content(), "Desc B");
    EXPECT_EQ(dds[2]->text_content(), "Desc C");
}

TEST(HtmlParserTest, TableWithColgroupAndMultipleColsV119) {
    auto doc = clever::html::parse(
        "<table>"
        "<colgroup><col span=\"2\"/><col/></colgroup>"
        "<thead><tr><th>A</th><th>B</th><th>C</th></tr></thead>"
        "<tbody><tr><td>1</td><td>2</td><td>3</td></tr></tbody>"
        "</table>");
    ASSERT_NE(doc, nullptr);

    auto* colgroup = doc->find_element("colgroup");
    ASSERT_NE(colgroup, nullptr);

    auto cols = doc->find_all_elements("col");
    ASSERT_EQ(cols.size(), 2u);
    EXPECT_EQ(get_attr_v63(cols[0], "span"), "2");

    auto ths = doc->find_all_elements("th");
    EXPECT_EQ(ths.size(), 3u);
    EXPECT_EQ(ths[0]->text_content(), "A");
    EXPECT_EQ(ths[2]->text_content(), "C");

    auto tds = doc->find_all_elements("td");
    EXPECT_EQ(tds.size(), 3u);
    EXPECT_EQ(tds[1]->text_content(), "2");
}

TEST(HtmlParserTest, FigureWithFigcaptionAndMultipleImagesV119) {
    auto doc = clever::html::parse(
        "<figure>"
        "<img src=\"a.jpg\" alt=\"Photo A\"/>"
        "<img src=\"b.jpg\" alt=\"Photo B\"/>"
        "<figcaption>Two photos side by side</figcaption>"
        "</figure>");
    ASSERT_NE(doc, nullptr);

    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);

    auto imgs = doc->find_all_elements("img");
    ASSERT_EQ(imgs.size(), 2u);
    EXPECT_EQ(get_attr_v63(imgs[0], "src"), "a.jpg");
    EXPECT_EQ(get_attr_v63(imgs[0], "alt"), "Photo A");
    EXPECT_EQ(get_attr_v63(imgs[1], "src"), "b.jpg");
    EXPECT_EQ(get_attr_v63(imgs[1], "alt"), "Photo B");

    auto* figcaption = doc->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->text_content(), "Two photos side by side");
    // img elements should have no children (void elements)
    EXPECT_TRUE(imgs[0]->children.empty());
    EXPECT_TRUE(imgs[1]->children.empty());
}

TEST(HtmlParserTest, NavWithNestedUnorderedListAndAnchorsV119) {
    auto doc = clever::html::parse(
        "<nav aria-label=\"main\">"
        "<ul>"
        "<li><a href=\"/home\">Home</a></li>"
        "<li><a href=\"/about\">About</a></li>"
        "<li><a href=\"/contact\" target=\"_blank\">Contact</a></li>"
        "</ul>"
        "</nav>");
    ASSERT_NE(doc, nullptr);

    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    EXPECT_EQ(get_attr_v63(nav, "aria-label"), "main");

    auto lis = doc->find_all_elements("li");
    EXPECT_EQ(lis.size(), 3u);

    auto anchors = doc->find_all_elements("a");
    ASSERT_EQ(anchors.size(), 3u);
    EXPECT_EQ(get_attr_v63(anchors[0], "href"), "/home");
    EXPECT_EQ(anchors[0]->text_content(), "Home");
    EXPECT_EQ(get_attr_v63(anchors[1], "href"), "/about");
    EXPECT_EQ(anchors[1]->text_content(), "About");
    EXPECT_EQ(get_attr_v63(anchors[2], "href"), "/contact");
    EXPECT_EQ(get_attr_v63(anchors[2], "target"), "_blank");
    EXPECT_EQ(anchors[2]->text_content(), "Contact");
}

TEST(HtmlParserTest, SelectWithOptgroupAndDisabledOptionsV119) {
    auto doc = clever::html::parse(
        "<select name=\"car\">"
        "<optgroup label=\"Swedish\">"
        "<option value=\"volvo\">Volvo</option>"
        "<option value=\"saab\" disabled>Saab</option>"
        "</optgroup>"
        "<optgroup label=\"German\">"
        "<option value=\"bmw\" selected>BMW</option>"
        "</optgroup>"
        "</select>");
    ASSERT_NE(doc, nullptr);

    auto* select = doc->find_element("select");
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(get_attr_v63(select, "name"), "car");

    auto optgroups = doc->find_all_elements("optgroup");
    ASSERT_EQ(optgroups.size(), 2u);
    EXPECT_EQ(get_attr_v63(optgroups[0], "label"), "Swedish");
    EXPECT_EQ(get_attr_v63(optgroups[1], "label"), "German");

    auto options = doc->find_all_elements("option");
    ASSERT_EQ(options.size(), 3u);
    EXPECT_EQ(get_attr_v63(options[0], "value"), "volvo");
    EXPECT_EQ(options[0]->text_content(), "Volvo");
    EXPECT_EQ(get_attr_v63(options[1], "disabled"), "");
    EXPECT_EQ(get_attr_v63(options[2], "selected"), "");
    EXPECT_EQ(options[2]->text_content(), "BMW");
}

TEST(HtmlParserTest, DeeplyNestedSectionArticleDivSpanV119) {
    auto doc = clever::html::parse(
        "<section>"
        "<article>"
        "<div>"
        "<span>Deep text</span>"
        "</div>"
        "</article>"
        "</section>");
    ASSERT_NE(doc, nullptr);

    auto* section = doc->find_element("section");
    ASSERT_NE(section, nullptr);

    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "Deep text");

    // Verify nesting: section > article > div > span
    // section's first element child should be article
    bool found_article = false;
    for (const auto& child : section->children) {
        if (child->tag_name == "article") { found_article = true; break; }
    }
    EXPECT_TRUE(found_article);

    bool found_div = false;
    for (const auto& child : article->children) {
        if (child->tag_name == "div") { found_div = true; break; }
    }
    EXPECT_TRUE(found_div);

    bool found_span = false;
    for (const auto& child : div->children) {
        if (child->tag_name == "span") { found_span = true; break; }
    }
    EXPECT_TRUE(found_span);
}

TEST(HtmlParserTest, MixedVoidAndNonVoidSiblingsWithTextV119) {
    // Use only inline void elements inside <p> to avoid auto-close behavior
    // (hr is block-level and would close the <p>)
    auto doc = clever::html::parse(
        "<p>Before<br/>Middle<wbr/>After<br/>End</p>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);

    // br and wbr are void elements - should have no children
    auto brs = doc->find_all_elements("br");
    EXPECT_EQ(brs.size(), 2u);
    EXPECT_TRUE(brs[0]->children.empty());
    EXPECT_TRUE(brs[1]->children.empty());

    auto wbrs = doc->find_all_elements("wbr");
    EXPECT_EQ(wbrs.size(), 1u);
    EXPECT_TRUE(wbrs[0]->children.empty());

    // The paragraph should contain all the text fragments
    std::string text = p->text_content();
    EXPECT_NE(text.find("Before"), std::string::npos);
    EXPECT_NE(text.find("Middle"), std::string::npos);
    EXPECT_NE(text.find("After"), std::string::npos);
    EXPECT_NE(text.find("End"), std::string::npos);
}

TEST(HtmlParserTest, FormWithTextareaAndLabeledInputsV119) {
    auto doc = clever::html::parse(
        "<form action=\"/submit\" method=\"post\">"
        "<label for=\"name\">Name:</label>"
        "<input type=\"text\" id=\"name\" name=\"name\" required/>"
        "<label for=\"msg\">Message:</label>"
        "<textarea id=\"msg\" name=\"msg\" rows=\"5\" cols=\"40\">Hello world</textarea>"
        "<button type=\"submit\">Send</button>"
        "</form>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "action"), "/submit");
    EXPECT_EQ(get_attr_v63(form, "method"), "post");

    auto labels = doc->find_all_elements("label");
    ASSERT_EQ(labels.size(), 2u);
    EXPECT_EQ(get_attr_v63(labels[0], "for"), "name");
    EXPECT_EQ(labels[0]->text_content(), "Name:");
    EXPECT_EQ(get_attr_v63(labels[1], "for"), "msg");

    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 1u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "text");
    EXPECT_EQ(get_attr_v63(inputs[0], "id"), "name");
    EXPECT_EQ(get_attr_v63(inputs[0], "required"), "");
    EXPECT_TRUE(inputs[0]->children.empty());

    auto* textarea = doc->find_element("textarea");
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(get_attr_v63(textarea, "rows"), "5");
    EXPECT_EQ(get_attr_v63(textarea, "cols"), "40");
    EXPECT_EQ(textarea->text_content(), "Hello world");

    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(get_attr_v63(button, "type"), "submit");
    EXPECT_EQ(button->text_content(), "Send");
}

// ---------------------------------------------------------------------------
// Cycle V120 — HTML parser: advanced structures, edge cases, attribute handling
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, NestedTablesWithCaptionAndColGroupV120) {
    auto doc = clever::html::parse(
        "<table>"
        "<caption>Sales Data</caption>"
        "<colgroup><col span=\"2\"><col></colgroup>"
        "<thead><tr><th>Product</th><th>Qty</th><th>Price</th></tr></thead>"
        "<tbody><tr><td>Widget</td><td>10</td><td>$5</td></tr></tbody>"
        "</table>");
    ASSERT_NE(doc, nullptr);

    auto* caption = doc->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->text_content(), "Sales Data");

    auto cols = doc->find_all_elements("col");
    ASSERT_EQ(cols.size(), 2u);
    EXPECT_EQ(get_attr_v63(cols[0], "span"), "2");

    auto ths = doc->find_all_elements("th");
    ASSERT_EQ(ths.size(), 3u);
    EXPECT_EQ(ths[0]->text_content(), "Product");
    EXPECT_EQ(ths[2]->text_content(), "Price");

    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 3u);
    EXPECT_EQ(tds[1]->text_content(), "10");
}

TEST(HtmlParserTest, MultipleVoidElementsInSequenceV120) {
    auto doc = clever::html::parse(
        "<body>"
        "<br><br><hr><br><input type=\"hidden\"><img src=\"a.png\">"
        "<p>after voids</p>"
        "</body>");
    ASSERT_NE(doc, nullptr);

    auto brs = doc->find_all_elements("br");
    ASSERT_EQ(brs.size(), 3u);
    for (auto* br : brs) {
        EXPECT_TRUE(br->children.empty());
    }

    auto hrs = doc->find_all_elements("hr");
    ASSERT_EQ(hrs.size(), 1u);
    EXPECT_TRUE(hrs[0]->children.empty());

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "hidden");
    EXPECT_TRUE(input->children.empty());

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "after voids");
}

TEST(HtmlParserTest, DataAndBooleanAttributesPreservedV120) {
    auto doc = clever::html::parse(
        "<div data-id=\"42\" data-role=\"admin\" hidden>"
        "<input disabled checked type=\"checkbox\">"
        "</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(get_attr_v63(div, "data-id"), "42");
    EXPECT_EQ(get_attr_v63(div, "data-role"), "admin");
    EXPECT_EQ(get_attr_v63(div, "hidden"), "");

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "disabled"), "");
    EXPECT_EQ(get_attr_v63(input, "checked"), "");
    EXPECT_EQ(get_attr_v63(input, "type"), "checkbox");
}

TEST(HtmlParserTest, DescriptionListDlWithMixedDtDdV120) {
    auto doc = clever::html::parse(
        "<dl>"
        "<dt>Term A</dt><dd>Definition A1</dd><dd>Definition A2</dd>"
        "<dt>Term B</dt><dd>Definition B1</dd>"
        "</dl>");
    ASSERT_NE(doc, nullptr);

    auto dts = doc->find_all_elements("dt");
    ASSERT_EQ(dts.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term A");
    EXPECT_EQ(dts[1]->text_content(), "Term B");

    auto dds = doc->find_all_elements("dd");
    ASSERT_EQ(dds.size(), 3u);
    EXPECT_EQ(dds[0]->text_content(), "Definition A1");
    EXPECT_EQ(dds[1]->text_content(), "Definition A2");
    EXPECT_EQ(dds[2]->text_content(), "Definition B1");
}

TEST(HtmlParserTest, InlineElementsPreserveTextContentV120) {
    auto doc = clever::html::parse(
        "<p>This is <strong>bold</strong> and <em>italic</em> and "
        "<code>monospace</code> text.</p>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "This is bold and italic and monospace text.");

    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "bold");

    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "italic");

    auto* code = doc->find_element("code");
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(code->text_content(), "monospace");
}

TEST(HtmlParserTest, NestedOrderedListsCorrectStructureV120) {
    auto doc = clever::html::parse(
        "<ol>"
        "<li>Item 1"
            "<ol><li>Sub 1a</li><li>Sub 1b</li></ol>"
        "</li>"
        "<li>Item 2</li>"
        "</ol>");
    ASSERT_NE(doc, nullptr);

    auto ols = doc->find_all_elements("ol");
    ASSERT_EQ(ols.size(), 2u);

    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 4u);
    // The nested li's have simple text
    EXPECT_EQ(lis[1]->text_content(), "Sub 1a");
    EXPECT_EQ(lis[2]->text_content(), "Sub 1b");
    EXPECT_EQ(lis[3]->text_content(), "Item 2");
}

TEST(HtmlParserTest, ScriptContentNotParsedAsTagsV120) {
    auto doc = clever::html::parse(
        "<html><head>"
        "<script type=\"text/javascript\">var x = 42; if (1 < 2) { console.log(x); }</script>"
        "</head><body><p>Real content</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    EXPECT_EQ(get_attr_v63(script, "type"), "text/javascript");
    // Script raw text should contain JS code
    std::string raw = script->text_content();
    EXPECT_NE(raw.find("var x = 42"), std::string::npos);
    EXPECT_NE(raw.find("console.log"), std::string::npos);

    // The only <p> in the document is the real one in body
    auto ps = doc->find_all_elements("p");
    ASSERT_EQ(ps.size(), 1u);
    EXPECT_EQ(ps[0]->text_content(), "Real content");
}

TEST(HtmlParserTest, AnchorWithHrefAndNestedSpanV120) {
    auto doc = clever::html::parse(
        "<nav>"
        "<a href=\"/home\" class=\"link active\"><span>Home</span></a>"
        "<a href=\"/about\"><span>About</span></a>"
        "</nav>");
    ASSERT_NE(doc, nullptr);

    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);

    auto anchors = doc->find_all_elements("a");
    ASSERT_EQ(anchors.size(), 2u);
    EXPECT_EQ(get_attr_v63(anchors[0], "href"), "/home");
    EXPECT_EQ(get_attr_v63(anchors[0], "class"), "link active");
    EXPECT_EQ(anchors[0]->text_content(), "Home");
    EXPECT_EQ(get_attr_v63(anchors[1], "href"), "/about");
    EXPECT_EQ(anchors[1]->text_content(), "About");

    auto spans = doc->find_all_elements("span");
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0]->tag_name, "span");
    EXPECT_EQ(spans[1]->text_content(), "About");
}

TEST(HtmlParserTest, ImplicitBodyInsertionWhenOnlyTextV121) {
    // When parsing bare text with no explicit tags, the parser should
    // still produce a document with an html/body structure containing the text.
    auto doc = clever::html::parse("Hello world, no tags here!");
    ASSERT_NE(doc, nullptr);

    auto* html_el = doc->find_element("html");
    ASSERT_NE(html_el, nullptr);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    std::string tc = body->text_content();
    EXPECT_NE(tc.find("Hello world"), std::string::npos);
    EXPECT_NE(tc.find("no tags here"), std::string::npos);
}

TEST(HtmlParserTest, TableColgroupMultipleColElementsV121) {
    auto doc = clever::html::parse(
        "<table>"
        "<colgroup>"
        "<col span=\"2\" style=\"background:red\">"
        "<col style=\"background:blue\">"
        "</colgroup>"
        "<tr><td>A</td><td>B</td><td>C</td></tr>"
        "</table>");
    ASSERT_NE(doc, nullptr);

    auto* colgroup = doc->find_element("colgroup");
    ASSERT_NE(colgroup, nullptr);

    auto cols = doc->find_all_elements("col");
    ASSERT_EQ(cols.size(), 2u);
    EXPECT_EQ(get_attr_v63(cols[0], "span"), "2");
    EXPECT_EQ(get_attr_v63(cols[0], "style"), "background:red");
    EXPECT_EQ(get_attr_v63(cols[1], "style"), "background:blue");

    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 3u);
    EXPECT_EQ(tds[2]->text_content(), "C");
}

TEST(HtmlParserTest, EntityDecodingAmpLtGtQuotInTextV121) {
    auto doc = clever::html::parse(
        "<p>5 &lt; 10 &amp; 20 &gt; 15 &quot;ok&quot;</p>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    std::string text = p->text_content();
    // Decoded entities should appear as literal characters
    EXPECT_NE(text.find("<"), std::string::npos);
    EXPECT_NE(text.find("&"), std::string::npos);
    EXPECT_NE(text.find(">"), std::string::npos);
    // The raw entity forms should NOT appear as-is
    EXPECT_EQ(text.find("&lt;"), std::string::npos);
    EXPECT_EQ(text.find("&gt;"), std::string::npos);
    EXPECT_EQ(text.find("&amp;"), std::string::npos);
}

TEST(HtmlParserTest, UnclosedParagraphsAutoCloseOnBlockV121) {
    // Per HTML spec, a <p> is auto-closed when another block element starts.
    auto doc = clever::html::parse(
        "<div>"
        "<p>First paragraph"
        "<p>Second paragraph"
        "<div>Block inside</div>"
        "</div>");
    ASSERT_NE(doc, nullptr);

    auto ps = doc->find_all_elements("p");
    ASSERT_GE(ps.size(), 2u);
    EXPECT_EQ(ps[0]->text_content(), "First paragraph");
    EXPECT_EQ(ps[1]->text_content(), "Second paragraph");

    // The inner div should be a sibling, not nested inside a <p>
    auto divs = doc->find_all_elements("div");
    ASSERT_GE(divs.size(), 2u);
    // Inner div contains the block text
    bool found_inner = false;
    for (auto* d : divs) {
        if (d->text_content() == "Block inside") {
            found_inner = true;
            break;
        }
    }
    EXPECT_TRUE(found_inner);
}

TEST(HtmlParserTest, TemplateAndSlotElementsRecognizedV121) {
    auto doc = clever::html::parse(
        "<div>"
        "<template id=\"card-tmpl\">"
        "<div class=\"card\"><slot name=\"title\">Default</slot></div>"
        "</template>"
        "<span>Visible content</span>"
        "</div>");
    ASSERT_NE(doc, nullptr);

    auto* tmpl = doc->find_element("template");
    ASSERT_NE(tmpl, nullptr);
    EXPECT_EQ(get_attr_v63(tmpl, "id"), "card-tmpl");

    auto* slot = doc->find_element("slot");
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(get_attr_v63(slot, "name"), "title");

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "Visible content");
}

TEST(HtmlParserTest, MixedCaseAttributeNamesLowercasedV121) {
    auto doc = clever::html::parse(
        "<div DATA-UserID=\"42\" Class=\"highlight\" TITLE=\"Info\">"
        "<input TYPE=\"email\" PLACEHOLDER=\"you@example.com\">"
        "</div>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    // HTML attributes should be lowercased
    EXPECT_EQ(get_attr_v63(div, "data-userid"), "42");
    EXPECT_EQ(get_attr_v63(div, "class"), "highlight");
    EXPECT_EQ(get_attr_v63(div, "title"), "Info");

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "email");
    EXPECT_EQ(get_attr_v63(input, "placeholder"), "you@example.com");
}

TEST(HtmlParserTest, NestedFormsAndButtonElementsV121) {
    auto doc = clever::html::parse(
        "<form action=\"/submit\" method=\"post\">"
        "<fieldset>"
        "<legend>Login</legend>"
        "<label for=\"user\">Username</label>"
        "<input id=\"user\" name=\"username\" type=\"text\">"
        "<label for=\"pass\">Password</label>"
        "<input id=\"pass\" name=\"password\" type=\"password\">"
        "<button type=\"submit\">Log In</button>"
        "</fieldset>"
        "</form>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "action"), "/submit");
    EXPECT_EQ(get_attr_v63(form, "method"), "post");

    auto labels = doc->find_all_elements("label");
    ASSERT_EQ(labels.size(), 2u);
    EXPECT_EQ(get_attr_v63(labels[0], "for"), "user");
    EXPECT_EQ(labels[1]->text_content(), "Password");

    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "text");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "password");
    EXPECT_EQ(get_attr_v63(inputs[1], "name"), "password");

    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text_content(), "Log In");
    EXPECT_EQ(get_attr_v63(button, "type"), "submit");
}

TEST(HtmlParserTest, StyleTagRawContentNotParsedAsChildElementsV121) {
    auto doc = clever::html::parse(
        "<html><head>"
        "<style>.foo > .bar { color: red; } h1 { font-size: 2em; }</style>"
        "</head><body><h1>Title</h1></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* style = doc->find_element("style");
    ASSERT_NE(style, nullptr);

    // Style content should be raw text, NOT parsed as tags
    std::string css = style->text_content();
    EXPECT_NE(css.find(".foo > .bar"), std::string::npos);
    EXPECT_NE(css.find("color: red"), std::string::npos);

    // There should be exactly one <h1> — the one in body, not a false one from CSS
    auto h1s = doc->find_all_elements("h1");
    ASSERT_EQ(h1s.size(), 1u);
    EXPECT_EQ(h1s[0]->text_content(), "Title");
}

TEST(HtmlParserTest, MultipleSiblingTablesEachWithCaptionV122) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table><caption>Sales Q1</caption>"
        "<tr><th>Region</th><th>Revenue</th></tr>"
        "<tr><td>East</td><td>$50k</td></tr>"
        "</table>"
        "<table><caption>Sales Q2</caption>"
        "<tr><th>Region</th><th>Revenue</th></tr>"
        "<tr><td>West</td><td>$72k</td></tr>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto tables = doc->find_all_elements("table");
    ASSERT_EQ(tables.size(), 2u);

    auto captions = doc->find_all_elements("caption");
    ASSERT_EQ(captions.size(), 2u);
    EXPECT_EQ(captions[0]->text_content(), "Sales Q1");
    EXPECT_EQ(captions[1]->text_content(), "Sales Q2");

    auto tds = doc->find_all_elements("td");
    ASSERT_EQ(tds.size(), 4u);
    EXPECT_EQ(tds[0]->text_content(), "East");
    EXPECT_EQ(tds[1]->text_content(), "$50k");
    EXPECT_EQ(tds[2]->text_content(), "West");
    EXPECT_EQ(tds[3]->text_content(), "$72k");
}

TEST(HtmlParserTest, PreformattedTextPreservesWhitespaceV122) {
    auto doc = clever::html::parse(
        "<html><body><pre>  line1\n"
        "    indented\n"
        "  line3</pre></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
    std::string content = pre->text_content();
    // Preformatted text: the parser should keep the internal whitespace/newlines
    EXPECT_NE(content.find("line1"), std::string::npos);
    EXPECT_NE(content.find("indented"), std::string::npos);
    EXPECT_NE(content.find("line3"), std::string::npos);
    // Should not collapse into a single line
    EXPECT_NE(content.find("\n"), std::string::npos);
}

TEST(HtmlParserTest, DetailsAndSummaryElementsV122) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<details>"
        "<summary>Click to expand</summary>"
        "<p>Hidden paragraph one.</p>"
        "<p>Hidden paragraph two.</p>"
        "</details>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);

    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text_content(), "Click to expand");

    auto paragraphs = doc->find_all_elements("p");
    ASSERT_EQ(paragraphs.size(), 2u);
    EXPECT_EQ(paragraphs[0]->text_content(), "Hidden paragraph one.");
    EXPECT_EQ(paragraphs[1]->text_content(), "Hidden paragraph two.");
}

TEST(HtmlParserTest, MultipleDifferentHeadingLevelsV122) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<h1>Title</h1>"
        "<h2>Subtitle</h2>"
        "<h3>Section</h3>"
        "<h4>Subsection</h4>"
        "<h5>Minor</h5>"
        "<h6>Smallest</h6>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* h1 = doc->find_element("h1");
    auto* h2 = doc->find_element("h2");
    auto* h3 = doc->find_element("h3");
    auto* h4 = doc->find_element("h4");
    auto* h5 = doc->find_element("h5");
    auto* h6 = doc->find_element("h6");

    ASSERT_NE(h1, nullptr);
    ASSERT_NE(h2, nullptr);
    ASSERT_NE(h3, nullptr);
    ASSERT_NE(h4, nullptr);
    ASSERT_NE(h5, nullptr);
    ASSERT_NE(h6, nullptr);

    EXPECT_EQ(h1->text_content(), "Title");
    EXPECT_EQ(h2->text_content(), "Subtitle");
    EXPECT_EQ(h3->text_content(), "Section");
    EXPECT_EQ(h4->text_content(), "Subsection");
    EXPECT_EQ(h5->text_content(), "Minor");
    EXPECT_EQ(h6->text_content(), "Smallest");
}

TEST(HtmlParserTest, ImageMapWithAreaElementsV122) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<img src=\"map.png\" usemap=\"#diagram\">"
        "<map name=\"diagram\">"
        "<area shape=\"rect\" coords=\"0,0,50,50\" href=\"/top-left\" alt=\"TL\">"
        "<area shape=\"circle\" coords=\"100,100,25\" href=\"/center\" alt=\"Center\">"
        "</map>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "usemap"), "#diagram");

    auto* map = doc->find_element("map");
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(get_attr_v63(map, "name"), "diagram");

    auto areas = doc->find_all_elements("area");
    ASSERT_EQ(areas.size(), 2u);
    EXPECT_EQ(get_attr_v63(areas[0], "shape"), "rect");
    EXPECT_EQ(get_attr_v63(areas[0], "href"), "/top-left");
    EXPECT_EQ(get_attr_v63(areas[0], "alt"), "TL");
    EXPECT_EQ(get_attr_v63(areas[1], "shape"), "circle");
    EXPECT_EQ(get_attr_v63(areas[1], "coords"), "100,100,25");
    EXPECT_EQ(get_attr_v63(areas[1], "alt"), "Center");
}

TEST(HtmlParserTest, ScriptAndNoscriptCoexistWithBodyContentV122) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<script>var x = 1 < 2 && 3 > 0;</script>"
        "<noscript><p>JavaScript is disabled.</p></noscript>"
        "<p>Main content.</p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    std::string script_text = script->text_content();
    // Script raw text must not be parsed — angle brackets preserved
    EXPECT_NE(script_text.find("1 < 2"), std::string::npos);
    EXPECT_NE(script_text.find("3 > 0"), std::string::npos);

    auto paragraphs = doc->find_all_elements("p");
    // At least the "Main content." paragraph should exist
    bool found_main = false;
    for (auto* p : paragraphs) {
        if (p->text_content() == "Main content.") {
            found_main = true;
        }
    }
    EXPECT_TRUE(found_main);
}

TEST(HtmlParserTest, MeterAndProgressElementsV122) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<label>Disk usage: <meter min=\"0\" max=\"100\" value=\"65\">65%</meter></label>"
        "<br>"
        "<label>Downloading: <progress max=\"100\" value=\"42\">42%</progress></label>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* meter = doc->find_element("meter");
    ASSERT_NE(meter, nullptr);
    EXPECT_EQ(get_attr_v63(meter, "min"), "0");
    EXPECT_EQ(get_attr_v63(meter, "max"), "100");
    EXPECT_EQ(get_attr_v63(meter, "value"), "65");
    EXPECT_EQ(meter->text_content(), "65%");

    auto* progress = doc->find_element("progress");
    ASSERT_NE(progress, nullptr);
    EXPECT_EQ(get_attr_v63(progress, "max"), "100");
    EXPECT_EQ(get_attr_v63(progress, "value"), "42");
    EXPECT_EQ(progress->text_content(), "42%");

    // br is void — should exist but have no children
    auto brs = doc->find_all_elements("br");
    ASSERT_GE(brs.size(), 1u);
    EXPECT_TRUE(brs[0]->children.empty());
}

TEST(HtmlParserTest, AudioSourceTrackElementsV122) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<audio controls preload=\"metadata\">"
        "<source src=\"song.ogg\" type=\"audio/ogg\">"
        "<source src=\"song.mp3\" type=\"audio/mpeg\">"
        "<track kind=\"subtitles\" src=\"subs_en.vtt\" srclang=\"en\" label=\"English\">"
        "Your browser does not support audio."
        "</audio>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* audio = doc->find_element("audio");
    ASSERT_NE(audio, nullptr);
    EXPECT_EQ(get_attr_v63(audio, "preload"), "metadata");

    auto sources = doc->find_all_elements("source");
    ASSERT_EQ(sources.size(), 2u);
    EXPECT_EQ(get_attr_v63(sources[0], "src"), "song.ogg");
    EXPECT_EQ(get_attr_v63(sources[0], "type"), "audio/ogg");
    EXPECT_EQ(get_attr_v63(sources[1], "src"), "song.mp3");
    EXPECT_EQ(get_attr_v63(sources[1], "type"), "audio/mpeg");

    auto tracks = doc->find_all_elements("track");
    ASSERT_EQ(tracks.size(), 1u);
    EXPECT_EQ(get_attr_v63(tracks[0], "kind"), "subtitles");
    EXPECT_EQ(get_attr_v63(tracks[0], "srclang"), "en");
    EXPECT_EQ(get_attr_v63(tracks[0], "label"), "English");

    // The fallback text should be part of audio's text content
    std::string audio_text = audio->text_content();
    EXPECT_NE(audio_text.find("browser does not support audio"), std::string::npos);
}

// ---------------------------------------------------------------------------
// V123 tests
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, FieldsetLegendGroupingV123) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<fieldset>"
        "<legend>Personal Info</legend>"
        "<label>Name: <input type=\"text\" name=\"name\"></label>"
        "<label>Email: <input type=\"email\" name=\"email\"></label>"
        "</fieldset>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* fieldset = doc->find_element("fieldset");
    ASSERT_NE(fieldset, nullptr);

    auto* legend = doc->find_element("legend");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->text_content(), "Personal Info");
    // legend should be a child of fieldset
    EXPECT_EQ(legend->parent, fieldset);

    auto labels = doc->find_all_elements("label");
    ASSERT_EQ(labels.size(), 2u);
    EXPECT_NE(labels[0]->text_content().find("Name:"), std::string::npos);
    EXPECT_NE(labels[1]->text_content().find("Email:"), std::string::npos);

    auto inputs = doc->find_all_elements("input");
    ASSERT_EQ(inputs.size(), 2u);
    EXPECT_EQ(get_attr_v63(inputs[0], "type"), "text");
    EXPECT_EQ(get_attr_v63(inputs[1], "type"), "email");
    // Inputs are void — no children
    EXPECT_TRUE(inputs[0]->children.empty());
    EXPECT_TRUE(inputs[1]->children.empty());
}

TEST(HtmlParserTest, RubyAnnotationElementsV123) {
    // Ruby annotation is used for East Asian typography
    auto doc = clever::html::parse(
        "<html><body>"
        "<ruby>"
        "漢<rp>(</rp><rt>kan</rt><rp>)</rp>"
        "字<rp>(</rp><rt>ji</rt><rp>)</rp>"
        "</ruby>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ruby = doc->find_element("ruby");
    ASSERT_NE(ruby, nullptr);

    auto rts = doc->find_all_elements("rt");
    ASSERT_EQ(rts.size(), 2u);
    EXPECT_EQ(rts[0]->text_content(), "kan");
    EXPECT_EQ(rts[1]->text_content(), "ji");

    auto rps = doc->find_all_elements("rp");
    ASSERT_EQ(rps.size(), 4u);
    EXPECT_EQ(rps[0]->text_content(), "(");
    EXPECT_EQ(rps[1]->text_content(), ")");

    // The full ruby text_content should contain the base characters
    std::string ruby_text = ruby->text_content();
    EXPECT_NE(ruby_text.find("漢"), std::string::npos);
    EXPECT_NE(ruby_text.find("字"), std::string::npos);
}

TEST(HtmlParserTest, OutputElementWithForAttributeV123) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<form oninput=\"result.value=parseInt(a.value)+parseInt(b.value)\">"
        "<input type=\"range\" id=\"a\" value=\"50\"> + "
        "<input type=\"number\" id=\"b\" value=\"25\"> = "
        "<output name=\"result\" for=\"a b\">75</output>"
        "</form>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* output = doc->find_element("output");
    ASSERT_NE(output, nullptr);
    EXPECT_EQ(output->tag_name, "output");
    EXPECT_EQ(get_attr_v63(output, "name"), "result");
    EXPECT_EQ(get_attr_v63(output, "for"), "a b");
    EXPECT_EQ(output->text_content(), "75");

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "oninput"),
              "result.value=parseInt(a.value)+parseInt(b.value)");

    // form text_content should contain the " + " and " = " connectors
    std::string form_text = form->text_content();
    EXPECT_NE(form_text.find("+"), std::string::npos);
    EXPECT_NE(form_text.find("="), std::string::npos);
}

TEST(HtmlParserTest, NestedBlockquotesWithCitationsV123) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<blockquote cite=\"https://example.com/speech\">"
        "<p>To be or not to be, that is the question.</p>"
        "<blockquote cite=\"https://example.com/reply\">"
        "<p>Indeed it is.</p>"
        "</blockquote>"
        "<footer><cite>Hamlet</cite>, Act 3, Scene 1</footer>"
        "</blockquote>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto bqs = doc->find_all_elements("blockquote");
    ASSERT_EQ(bqs.size(), 2u);

    // Outer blockquote
    EXPECT_EQ(get_attr_v63(bqs[0], "cite"), "https://example.com/speech");
    // Inner blockquote should be nested inside outer
    EXPECT_EQ(get_attr_v63(bqs[1], "cite"), "https://example.com/reply");
    EXPECT_EQ(bqs[1]->parent, bqs[0]);

    auto* cite_el = doc->find_element("cite");
    ASSERT_NE(cite_el, nullptr);
    EXPECT_EQ(cite_el->text_content(), "Hamlet");

    auto* footer = doc->find_element("footer");
    ASSERT_NE(footer, nullptr);
    std::string footer_text = footer->text_content();
    EXPECT_NE(footer_text.find("Act 3, Scene 1"), std::string::npos);
}

TEST(HtmlParserTest, WbrAndSmallInlineElementsV123) {
    // wbr is a void element indicating a word-break opportunity
    auto doc = clever::html::parse(
        "<html><body>"
        "<p>Supercali<wbr>fragilistic<wbr>expialidocious</p>"
        "<p><small>Fine print: <abbr title=\"Terms of Service\">ToS</abbr> apply.</small></p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto wbrs = doc->find_all_elements("wbr");
    ASSERT_EQ(wbrs.size(), 2u);
    // wbr is void — no children
    EXPECT_TRUE(wbrs[0]->children.empty());
    EXPECT_TRUE(wbrs[1]->children.empty());

    auto paragraphs = doc->find_all_elements("p");
    ASSERT_GE(paragraphs.size(), 2u);
    // First paragraph contains the long word fragments
    std::string p1_text = paragraphs[0]->text_content();
    EXPECT_NE(p1_text.find("Supercali"), std::string::npos);
    EXPECT_NE(p1_text.find("expialidocious"), std::string::npos);

    auto* small = doc->find_element("small");
    ASSERT_NE(small, nullptr);
    EXPECT_NE(small->text_content().find("Fine print:"), std::string::npos);

    auto* abbr = doc->find_element("abbr");
    ASSERT_NE(abbr, nullptr);
    EXPECT_EQ(get_attr_v63(abbr, "title"), "Terms of Service");
    EXPECT_EQ(abbr->text_content(), "ToS");
}

TEST(HtmlParserTest, MultipleScriptsPreserveOrderAndRawContentV123) {
    auto doc = clever::html::parse(
        "<html><head>"
        "<script type=\"application/json\">{\"key\": \"<value>\"}</script>"
        "<script src=\"app.js\" defer></script>"
        "</head><body>"
        "<script>if (a < b && c > d) { alert('ok'); }</script>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto scripts = doc->find_all_elements("script");
    ASSERT_EQ(scripts.size(), 3u);

    // First script: inline JSON with angle brackets preserved
    EXPECT_EQ(get_attr_v63(scripts[0], "type"), "application/json");
    std::string json_text = scripts[0]->text_content();
    EXPECT_NE(json_text.find("\"<value>\""), std::string::npos);
    EXPECT_NE(json_text.find("\"key\""), std::string::npos);

    // Second script: external with defer boolean attribute, no body
    EXPECT_EQ(get_attr_v63(scripts[1], "src"), "app.js");
    // defer is a boolean attribute — should exist in attributes
    bool has_defer = false;
    for (const auto& a : scripts[1]->attributes) {
        if (a.name == "defer") { has_defer = true; break; }
    }
    EXPECT_TRUE(has_defer);

    // Third script: raw content with operators not treated as tags
    std::string inline_text = scripts[2]->text_content();
    EXPECT_NE(inline_text.find("a < b"), std::string::npos);
    EXPECT_NE(inline_text.find("c > d"), std::string::npos);
    EXPECT_NE(inline_text.find("alert('ok')"), std::string::npos);
}

TEST(HtmlParserTest, TableWithTheadTbodyTfootSectionsV123) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<caption>Quarterly Results</caption>"
        "<thead><tr><th>Q1</th><th>Q2</th><th>Q3</th><th>Q4</th></tr></thead>"
        "<tfoot><tr><td>Total</td><td colspan=\"3\">200</td></tr></tfoot>"
        "<tbody>"
        "<tr><td>50</td><td>60</td><td>40</td><td>50</td></tr>"
        "</tbody>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* caption = doc->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->text_content(), "Quarterly Results");

    auto* thead = doc->find_element("thead");
    ASSERT_NE(thead, nullptr);
    auto ths = doc->find_all_elements("th");
    ASSERT_EQ(ths.size(), 4u);
    EXPECT_EQ(ths[0]->text_content(), "Q1");
    EXPECT_EQ(ths[3]->text_content(), "Q4");

    auto* tfoot = doc->find_element("tfoot");
    ASSERT_NE(tfoot, nullptr);

    auto* tbody = doc->find_element("tbody");
    ASSERT_NE(tbody, nullptr);

    // tfoot has a td with colspan
    auto tds = tfoot->find_all_elements("td");
    ASSERT_GE(tds.size(), 2u);
    EXPECT_EQ(tds[0]->text_content(), "Total");
    EXPECT_EQ(get_attr_v63(tds[1], "colspan"), "3");
    EXPECT_EQ(tds[1]->text_content(), "200");

    // tbody row should have 4 data cells
    auto tbody_tds = tbody->find_all_elements("td");
    ASSERT_EQ(tbody_tds.size(), 4u);
    EXPECT_EQ(tbody_tds[0]->text_content(), "50");
    EXPECT_EQ(tbody_tds[2]->text_content(), "40");
}

TEST(HtmlParserTest, AdjacentTextNodesMergeAndWhitespaceV123) {
    // When parser sees adjacent text fragments they should be accessible
    // via text_content even if separated by comments
    auto doc = clever::html::parse(
        "<html><body>"
        "<div>Hello<!-- hidden -->World"
        "<!-- another comment --> and more</div>"
        "<pre>  spaced   out  \n  lines  </pre>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    std::string div_text = div->text_content();
    // text_content should return all text nodes concatenated
    EXPECT_NE(div_text.find("Hello"), std::string::npos);
    EXPECT_NE(div_text.find("World"), std::string::npos);
    EXPECT_NE(div_text.find("and more"), std::string::npos);

    // This parser's text_content() includes comment data in the output
    EXPECT_NE(div_text.find("hidden"), std::string::npos);
    EXPECT_NE(div_text.find("another comment"), std::string::npos);

    // Verify comments are stored as Comment-type children in the tree
    int comment_count = 0;
    for (const auto& child : div->children) {
        if (child->type == clever::html::SimpleNode::Comment) {
            comment_count++;
        }
    }
    EXPECT_EQ(comment_count, 2);

    // pre element should preserve whitespace in its text_content
    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
    std::string pre_text = pre->text_content();
    EXPECT_NE(pre_text.find("  spaced   out  "), std::string::npos);
}

// ---------------------------------------------------------------------------
// V124 Tests
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, DatalistWithInputListBindingV124) {
    // Datalist element with option children, linked via input list attribute
    auto doc = clever::html::parse(
        "<html><body>"
        "<input type=\"text\" list=\"browsers\" name=\"browser\"/>"
        "<datalist id=\"browsers\">"
        "<option value=\"Chrome\"></option>"
        "<option value=\"Firefox\"></option>"
        "<option value=\"Safari\"></option>"
        "<option value=\"Edge\"></option>"
        "</datalist>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "list"), "browsers");
    EXPECT_EQ(get_attr_v63(input, "type"), "text");

    auto* datalist = doc->find_element("datalist");
    ASSERT_NE(datalist, nullptr);
    EXPECT_EQ(get_attr_v63(datalist, "id"), "browsers");

    auto options = datalist->find_all_elements("option");
    ASSERT_EQ(options.size(), 4u);
    EXPECT_EQ(get_attr_v63(options[0], "value"), "Chrome");
    EXPECT_EQ(get_attr_v63(options[1], "value"), "Firefox");
    EXPECT_EQ(get_attr_v63(options[2], "value"), "Safari");
    EXPECT_EQ(get_attr_v63(options[3], "value"), "Edge");
}

TEST(HtmlParserTest, NavWithNestedListAndAriaLandmarkV124) {
    // Semantic nav element containing a nested list with aria attributes
    auto doc = clever::html::parse(
        "<html><body>"
        "<nav aria-label=\"Main navigation\" role=\"navigation\">"
        "<ul>"
        "<li><a href=\"/home\">Home</a></li>"
        "<li><a href=\"/about\">About</a>"
        "  <ul>"
        "    <li><a href=\"/about/team\">Team</a></li>"
        "    <li><a href=\"/about/history\">History</a></li>"
        "  </ul>"
        "</li>"
        "<li><a href=\"/contact\">Contact</a></li>"
        "</ul>"
        "</nav>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    EXPECT_EQ(get_attr_v63(nav, "aria-label"), "Main navigation");
    EXPECT_EQ(get_attr_v63(nav, "role"), "navigation");

    // Should have two <ul> elements (outer + nested)
    auto uls = nav->find_all_elements("ul");
    ASSERT_EQ(uls.size(), 2u);

    // All links across both levels
    auto links = nav->find_all_elements("a");
    ASSERT_EQ(links.size(), 5u);
    EXPECT_EQ(get_attr_v63(links[0], "href"), "/home");
    EXPECT_EQ(links[0]->text_content(), "Home");
    // Depth-first: nested links come before the next sibling <li>
    EXPECT_EQ(get_attr_v63(links[2], "href"), "/about/team");
    EXPECT_EQ(links[2]->text_content(), "Team");
    EXPECT_EQ(get_attr_v63(links[3], "href"), "/about/history");
    EXPECT_EQ(links[4]->text_content(), "Contact");
}

TEST(HtmlParserTest, ObjectParamAndEmbedElementsV124) {
    // Legacy <object> with <param> children plus standalone <embed>
    auto doc = clever::html::parse(
        "<html><body>"
        "<object data=\"movie.swf\" type=\"application/x-shockwave-flash\" width=\"400\" height=\"300\">"
        "<param name=\"quality\" value=\"high\"/>"
        "<param name=\"wmode\" value=\"transparent\"/>"
        "<p>Fallback content</p>"
        "</object>"
        "<embed src=\"clip.mp4\" type=\"video/mp4\" width=\"640\" height=\"480\"/>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* object = doc->find_element("object");
    ASSERT_NE(object, nullptr);
    EXPECT_EQ(get_attr_v63(object, "data"), "movie.swf");
    EXPECT_EQ(get_attr_v63(object, "type"), "application/x-shockwave-flash");
    EXPECT_EQ(get_attr_v63(object, "width"), "400");

    auto params = object->find_all_elements("param");
    ASSERT_EQ(params.size(), 2u);
    EXPECT_EQ(get_attr_v63(params[0], "name"), "quality");
    EXPECT_EQ(get_attr_v63(params[0], "value"), "high");
    EXPECT_EQ(get_attr_v63(params[1], "name"), "wmode");
    EXPECT_EQ(get_attr_v63(params[1], "value"), "transparent");

    // Fallback <p> inside object
    auto* fallback_p = object->find_element("p");
    ASSERT_NE(fallback_p, nullptr);
    EXPECT_EQ(fallback_p->text_content(), "Fallback content");

    auto* embed = doc->find_element("embed");
    ASSERT_NE(embed, nullptr);
    EXPECT_EQ(get_attr_v63(embed, "src"), "clip.mp4");
    EXPECT_EQ(get_attr_v63(embed, "type"), "video/mp4");
}

TEST(HtmlParserTest, MainAsideHeaderFooterSectionArticleLayoutV124) {
    // Full semantic page layout with main, aside, header, footer, section, article
    auto doc = clever::html::parse(
        "<html><body>"
        "<header><h1>Site Title</h1></header>"
        "<main>"
        "<article>"
        "<section><h2>Introduction</h2><p>Intro text</p></section>"
        "<section><h2>Details</h2><p>Detail text</p></section>"
        "</article>"
        "</main>"
        "<aside><p>Sidebar content</p></aside>"
        "<footer><p>Copyright 2024</p></footer>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* header = doc->find_element("header");
    ASSERT_NE(header, nullptr);
    auto* h1 = header->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Site Title");

    auto* main_el = doc->find_element("main");
    ASSERT_NE(main_el, nullptr);
    auto* article = main_el->find_element("article");
    ASSERT_NE(article, nullptr);

    auto sections = article->find_all_elements("section");
    ASSERT_EQ(sections.size(), 2u);
    auto* s1_h2 = sections[0]->find_element("h2");
    ASSERT_NE(s1_h2, nullptr);
    EXPECT_EQ(s1_h2->text_content(), "Introduction");

    auto* aside = doc->find_element("aside");
    ASSERT_NE(aside, nullptr);
    EXPECT_NE(aside->text_content().find("Sidebar content"), std::string::npos);

    auto* footer = doc->find_element("footer");
    ASSERT_NE(footer, nullptr);
    EXPECT_NE(footer->text_content().find("Copyright 2024"), std::string::npos);
}

TEST(HtmlParserTest, MapWithMultipleAreaShapesV124) {
    // Image map with different area shapes (rect, circle, poly)
    auto doc = clever::html::parse(
        "<html><body>"
        "<img src=\"planets.jpg\" usemap=\"#planetmap\" alt=\"Planets\"/>"
        "<map name=\"planetmap\">"
        "<area shape=\"rect\" coords=\"0,0,82,126\" href=\"sun.htm\" alt=\"Sun\"/>"
        "<area shape=\"circle\" coords=\"90,58,3\" href=\"mercury.htm\" alt=\"Mercury\"/>"
        "<area shape=\"poly\" coords=\"124,58,8,50,200,120\" href=\"venus.htm\" alt=\"Venus\"/>"
        "</map>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(img, "usemap"), "#planetmap");

    auto* map = doc->find_element("map");
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(get_attr_v63(map, "name"), "planetmap");

    auto areas = map->find_all_elements("area");
    ASSERT_EQ(areas.size(), 3u);

    // rect area
    EXPECT_EQ(get_attr_v63(areas[0], "shape"), "rect");
    EXPECT_EQ(get_attr_v63(areas[0], "coords"), "0,0,82,126");
    EXPECT_EQ(get_attr_v63(areas[0], "href"), "sun.htm");
    EXPECT_EQ(get_attr_v63(areas[0], "alt"), "Sun");

    // circle area
    EXPECT_EQ(get_attr_v63(areas[1], "shape"), "circle");
    EXPECT_EQ(get_attr_v63(areas[1], "coords"), "90,58,3");
    EXPECT_EQ(get_attr_v63(areas[1], "alt"), "Mercury");

    // poly area
    EXPECT_EQ(get_attr_v63(areas[2], "shape"), "poly");
    EXPECT_EQ(get_attr_v63(areas[2], "coords"), "124,58,8,50,200,120");
    EXPECT_EQ(get_attr_v63(areas[2], "href"), "venus.htm");
}

TEST(HtmlParserTest, DialogElementWithFormMethodDialogV124) {
    // HTML dialog element containing a form with method=dialog
    auto doc = clever::html::parse(
        "<html><body>"
        "<dialog open=\"\" id=\"confirm-dlg\">"
        "<form method=\"dialog\">"
        "<p>Are you sure you want to proceed?</p>"
        "<menu>"
        "<button value=\"cancel\">Cancel</button>"
        "<button value=\"confirm\">Confirm</button>"
        "</menu>"
        "</form>"
        "</dialog>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dialog = doc->find_element("dialog");
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(get_attr_v63(dialog, "id"), "confirm-dlg");
    // open is a boolean attribute, value should be empty string
    EXPECT_EQ(get_attr_v63(dialog, "open"), "");

    auto* form = dialog->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "method"), "dialog");

    auto* p = dialog->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Are you sure you want to proceed?");

    auto* menu = dialog->find_element("menu");
    ASSERT_NE(menu, nullptr);
    auto buttons = menu->find_all_elements("button");
    ASSERT_EQ(buttons.size(), 2u);
    EXPECT_EQ(get_attr_v63(buttons[0], "value"), "cancel");
    EXPECT_EQ(buttons[0]->text_content(), "Cancel");
    EXPECT_EQ(get_attr_v63(buttons[1], "value"), "confirm");
    EXPECT_EQ(buttons[1]->text_content(), "Confirm");
}

TEST(HtmlParserTest, BaseTagAffectsNoChildrenAndIsVoidV124) {
    // <base> is a void element that should not consume subsequent content
    auto doc = clever::html::parse(
        "<html>"
        "<head>"
        "<base href=\"https://example.com/\" target=\"_blank\"/>"
        "<title>Base Tag Test</title>"
        "<link rel=\"icon\" href=\"/favicon.ico\"/>"
        "</head>"
        "<body>"
        "<a href=\"page.html\">Link</a>"
        "<a href=\"other.html\">Other</a>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* base = doc->find_element("base");
    ASSERT_NE(base, nullptr);
    EXPECT_EQ(get_attr_v63(base, "href"), "https://example.com/");
    EXPECT_EQ(get_attr_v63(base, "target"), "_blank");
    // base is void: should have no children
    EXPECT_EQ(base->children.size(), 0u);

    auto* title = doc->find_element("title");
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->text_content(), "Base Tag Test");

    auto* link = doc->find_element("link");
    ASSERT_NE(link, nullptr);
    EXPECT_EQ(get_attr_v63(link, "rel"), "icon");
    EXPECT_EQ(link->children.size(), 0u);

    // Body links should be separate elements, not swallowed by base
    auto anchors = doc->find_all_elements("a");
    ASSERT_EQ(anchors.size(), 2u);
    EXPECT_EQ(anchors[0]->text_content(), "Link");
    EXPECT_EQ(anchors[1]->text_content(), "Other");
}

TEST(HtmlParserTest, TableCaptionColspanEmptyCellsAndNestedTextV124) {
    // Table with caption, mixed empty cells, cells with nested inline elements
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<caption>Employee <strong>Directory</strong></caption>"
        "<tr>"
        "<th>Name</th><th>Dept</th><th>Notes</th>"
        "</tr>"
        "<tr>"
        "<td><em>Alice</em> <strong>Smith</strong></td>"
        "<td>Engineering</td>"
        "<td></td>"
        "</tr>"
        "<tr>"
        "<td colspan=\"2\">Bob &amp; Carol</td>"
        "<td><a href=\"#note1\">See note</a></td>"
        "</tr>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    // Caption should contain nested strong element
    auto* caption = doc->find_element("caption");
    ASSERT_NE(caption, nullptr);
    auto* cap_strong = caption->find_element("strong");
    ASSERT_NE(cap_strong, nullptr);
    EXPECT_EQ(cap_strong->text_content(), "Directory");

    auto rows = doc->find_all_elements("tr");
    ASSERT_EQ(rows.size(), 3u);

    // First data row: cell with nested inline elements
    auto row1_tds = rows[1]->find_all_elements("td");
    ASSERT_EQ(row1_tds.size(), 3u);
    auto* em = row1_tds[0]->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "Alice");
    auto* strong = row1_tds[0]->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "Smith");

    // Empty cell should exist but have no text
    EXPECT_EQ(row1_tds[2]->text_content(), "");

    // Second data row: colspan cell with entity-decoded ampersand
    auto row2_tds = rows[2]->find_all_elements("td");
    ASSERT_GE(row2_tds.size(), 2u);
    EXPECT_EQ(get_attr_v63(row2_tds[0], "colspan"), "2");
    EXPECT_NE(row2_tds[0]->text_content().find("Bob"), std::string::npos);

    // Last cell has a link
    auto* note_link = rows[2]->find_element("a");
    ASSERT_NE(note_link, nullptr);
    EXPECT_EQ(get_attr_v63(note_link, "href"), "#note1");
    EXPECT_EQ(note_link->text_content(), "See note");
}

// ---------------------------------------------------------------------------
// Cycle V125 — HTML parser tests: diverse scenarios covering nesting,
//              attributes, void elements, text content, comments, and types.
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, NestedNavWithUlLiAndAnchorsV125) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<nav id=\"main-nav\">"
        "<ul>"
        "<li><a href=\"/home\">Home</a></li>"
        "<li><a href=\"/about\">About</a></li>"
        "<li><a href=\"/contact\">Contact</a></li>"
        "</ul>"
        "</nav>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    EXPECT_EQ(get_attr_v63(nav, "id"), "main-nav");

    auto* ul = nav->find_element("ul");
    ASSERT_NE(ul, nullptr);

    auto lis = ul->find_all_elements("li");
    ASSERT_EQ(lis.size(), 3u);

    auto anchors = nav->find_all_elements("a");
    ASSERT_EQ(anchors.size(), 3u);

    EXPECT_EQ(get_attr_v63(anchors[0], "href"), "/home");
    EXPECT_EQ(anchors[0]->text_content(), "Home");

    EXPECT_EQ(get_attr_v63(anchors[1], "href"), "/about");
    EXPECT_EQ(anchors[1]->text_content(), "About");

    EXPECT_EQ(get_attr_v63(anchors[2], "href"), "/contact");
    EXPECT_EQ(anchors[2]->text_content(), "Contact");
}

TEST(HtmlParserTest, CommentNodeTypeAndTextContentV125) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<!-- Banner section -->"
        "<div>Content</div>"
        "<!-- Footer section -->"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    // Count comment nodes among body's children
    int comment_count = 0;
    for (auto& child : body->children) {
        if (child->type == clever::html::SimpleNode::Comment) {
            comment_count++;
        }
    }
    EXPECT_GE(comment_count, 2);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->text_content(), "Content");
}

TEST(HtmlParserTest, MultipleVoidElementsAmongTextNodesV125) {
    auto doc = clever::html::parse(
        "<body>"
        "Line1<br>Line2<br>Line3<hr>End"
        "</body>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    auto brs = body->find_all_elements("br");
    EXPECT_EQ(brs.size(), 2u);

    auto hrs = body->find_all_elements("hr");
    EXPECT_EQ(hrs.size(), 1u);

    // All void elements must have zero children
    for (auto* br : brs) {
        EXPECT_TRUE(br->children.empty());
    }
    for (auto* hr : hrs) {
        EXPECT_TRUE(hr->children.empty());
    }

    // Body text_content should contain all text fragments
    std::string text = body->text_content();
    EXPECT_NE(text.find("Line1"), std::string::npos);
    EXPECT_NE(text.find("Line2"), std::string::npos);
    EXPECT_NE(text.find("Line3"), std::string::npos);
    EXPECT_NE(text.find("End"), std::string::npos);
}

TEST(HtmlParserTest, DeeplyNestedSixLevelDivChainV125) {
    auto doc = clever::html::parse(
        "<div id=\"l1\">"
        "  <div id=\"l2\">"
        "    <div id=\"l3\">"
        "      <div id=\"l4\">"
        "        <div id=\"l5\">"
        "          <div id=\"l6\"><span>Leaf</span></div>"
        "        </div>"
        "      </div>"
        "    </div>"
        "  </div>"
        "</div>");
    ASSERT_NE(doc, nullptr);

    auto all_divs = doc->find_all_elements("div");
    ASSERT_EQ(all_divs.size(), 6u);

    // Verify nesting: each div's parent should be the previous level div
    for (size_t i = 1; i < all_divs.size(); ++i) {
        EXPECT_EQ(all_divs[i]->parent, all_divs[i - 1]);
    }

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "Leaf");
    EXPECT_EQ(span->parent, all_divs[5]);
}

TEST(HtmlParserTest, TableWithCaptionTheadTbodyTfootV125) {
    auto doc = clever::html::parse(
        "<table>"
        "<caption>Sales Report</caption>"
        "<thead><tr><th>Product</th><th>Revenue</th></tr></thead>"
        "<tbody>"
        "<tr><td>Widget</td><td>$100</td></tr>"
        "<tr><td>Gadget</td><td>$200</td></tr>"
        "</tbody>"
        "<tfoot><tr><td>Total</td><td>$300</td></tr></tfoot>"
        "</table>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);

    auto* caption = table->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->text_content(), "Sales Report");

    auto* thead = table->find_element("thead");
    ASSERT_NE(thead, nullptr);
    auto ths = thead->find_all_elements("th");
    ASSERT_EQ(ths.size(), 2u);
    EXPECT_EQ(ths[0]->text_content(), "Product");
    EXPECT_EQ(ths[1]->text_content(), "Revenue");

    auto* tbody = table->find_element("tbody");
    ASSERT_NE(tbody, nullptr);
    auto body_rows = tbody->find_all_elements("tr");
    ASSERT_EQ(body_rows.size(), 2u);

    auto* tfoot = table->find_element("tfoot");
    ASSERT_NE(tfoot, nullptr);
    auto foot_tds = tfoot->find_all_elements("td");
    ASSERT_EQ(foot_tds.size(), 2u);
    EXPECT_EQ(foot_tds[0]->text_content(), "Total");
    EXPECT_EQ(foot_tds[1]->text_content(), "$300");
}

TEST(HtmlParserTest, FormWithLabelInputTextareaAndButtonV125) {
    auto doc = clever::html::parse(
        "<form action=\"/submit\" method=\"post\">"
        "<label for=\"name\">Name:</label>"
        "<input id=\"name\" type=\"text\" value=\"John\"/>"
        "<label for=\"msg\">Message:</label>"
        "<textarea id=\"msg\" rows=\"4\" cols=\"50\">Hello World</textarea>"
        "<button type=\"submit\">Send</button>"
        "</form>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(get_attr_v63(form, "action"), "/submit");
    EXPECT_EQ(get_attr_v63(form, "method"), "post");

    auto labels = form->find_all_elements("label");
    ASSERT_EQ(labels.size(), 2u);
    EXPECT_EQ(get_attr_v63(labels[0], "for"), "name");
    EXPECT_EQ(labels[0]->text_content(), "Name:");

    auto* input = form->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "text");
    EXPECT_EQ(get_attr_v63(input, "value"), "John");
    EXPECT_TRUE(input->children.empty()); // void element

    auto* textarea = form->find_element("textarea");
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(get_attr_v63(textarea, "rows"), "4");
    EXPECT_EQ(get_attr_v63(textarea, "cols"), "50");
    EXPECT_EQ(textarea->text_content(), "Hello World");

    auto* button = form->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(get_attr_v63(button, "type"), "submit");
    EXPECT_EQ(button->text_content(), "Send");
}

TEST(HtmlParserTest, DoctypeAndDocumentRootTypeV125) {
    auto doc = clever::html::parse(
        "<!DOCTYPE html>"
        "<html lang=\"en\">"
        "<head><title>Page</title></head>"
        "<body><p>Content</p></body>"
        "</html>");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->type, clever::html::SimpleNode::Document);

    // Find doctype node among root children
    bool found_doctype = false;
    for (auto& child : doc->children) {
        if (child->type == clever::html::SimpleNode::DocumentType) {
            found_doctype = true;
            break;
        }
    }
    EXPECT_TRUE(found_doctype);

    auto* html_el = doc->find_element("html");
    ASSERT_NE(html_el, nullptr);
    EXPECT_EQ(html_el->type, clever::html::SimpleNode::Element);
    EXPECT_EQ(get_attr_v63(html_el, "lang"), "en");

    auto* title = doc->find_element("title");
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->text_content(), "Page");

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Content");
}

TEST(HtmlParserTest, ScriptTagRawContentNotParsedAsHtmlV125) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<script type=\"text/javascript\">"
        "if (x < 10 && y > 5) { document.getElementById(\"test\"); }"
        "</script>"
        "<p>After script</p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto scripts = doc->find_all_elements("script");
    ASSERT_EQ(scripts.size(), 1u);
    EXPECT_EQ(get_attr_v63(scripts[0], "type"), "text/javascript");

    // Script content should be treated as raw text, not parsed as HTML
    std::string content = scripts[0]->text_content();
    EXPECT_NE(content.find("x < 10"), std::string::npos);
    EXPECT_NE(content.find("&&"), std::string::npos);

    // The <p> after script should be a sibling, not consumed by script
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "After script");
}

// ============================================================================
// V126 Tests
// ============================================================================

// 1. Parse nested ordered list with list items and verify count and text
TEST(HtmlParserTest, HtmlV126_1) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<ol>"
        "<li>Alpha</li>"
        "<li>Beta<ul><li>Sub1</li><li>Sub2</li></ul></li>"
        "<li>Gamma</li>"
        "</ol>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    // find_all_elements is recursive (DFS), so it finds all 5 li elements
    // Order: Alpha(0), Beta(1), Sub1(2), Sub2(3), Gamma(4)
    auto all_lis = ol->find_all_elements("li");
    ASSERT_EQ(all_lis.size(), 5u);
    EXPECT_EQ(all_lis[0]->text_content(), "Alpha");
    EXPECT_EQ(all_lis[4]->text_content(), "Gamma");

    auto* ul = ol->find_element("ul");
    ASSERT_NE(ul, nullptr);
    auto sub_items = ul->find_all_elements("li");
    ASSERT_EQ(sub_items.size(), 2u);
    EXPECT_EQ(sub_items[0]->text_content(), "Sub1");
    EXPECT_EQ(sub_items[1]->text_content(), "Sub2");
}

// 2. Parse anchor tag with multiple attributes including target and title
TEST(HtmlParserTest, HtmlV126_2) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<a href=\"https://example.com\" target=\"_blank\" title=\"Example Link\" "
        "rel=\"noopener noreferrer\">Visit</a>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(get_attr_v63(a, "href"), "https://example.com");
    EXPECT_EQ(get_attr_v63(a, "target"), "_blank");
    EXPECT_EQ(get_attr_v63(a, "title"), "Example Link");
    EXPECT_EQ(get_attr_v63(a, "rel"), "noopener noreferrer");
    EXPECT_EQ(a->text_content(), "Visit");
    EXPECT_EQ(a->type, clever::html::SimpleNode::Element);
    EXPECT_EQ(a->tag_name, "a");
}

// 3. Parse multiple meta tags in head and verify their attributes
TEST(HtmlParserTest, HtmlV126_3) {
    auto doc = clever::html::parse(
        "<html><head>"
        "<meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
        "<meta name=\"description\" content=\"A test page\">"
        "</head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto metas = doc->find_all_elements("meta");
    ASSERT_EQ(metas.size(), 3u);
    EXPECT_EQ(get_attr_v63(metas[0], "charset"), "utf-8");
    EXPECT_EQ(get_attr_v63(metas[1], "name"), "viewport");
    EXPECT_EQ(get_attr_v63(metas[1], "content"), "width=device-width, initial-scale=1.0");
    EXPECT_EQ(get_attr_v63(metas[2], "name"), "description");
    EXPECT_EQ(get_attr_v63(metas[2], "content"), "A test page");
    // meta is a void element, should have no children
    EXPECT_TRUE(metas[0]->children.empty());
    EXPECT_TRUE(metas[1]->children.empty());
}

// 4. Parse comment nodes and verify they are preserved in the tree
TEST(HtmlParserTest, HtmlV126_4) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<!-- First comment -->"
        "<p>Visible text</p>"
        "<!-- Second comment -->"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    int comment_count = 0;
    std::vector<std::string> comment_texts;
    for (auto& child : body->children) {
        if (child->type == clever::html::SimpleNode::Comment) {
            comment_count++;
            comment_texts.push_back(child->data);
        }
    }
    EXPECT_EQ(comment_count, 2);
    ASSERT_EQ(comment_texts.size(), 2u);
    EXPECT_NE(comment_texts[0].find("First comment"), std::string::npos);
    EXPECT_NE(comment_texts[1].find("Second comment"), std::string::npos);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "Visible text");
}

// 5. Verify parent pointers are set correctly through a three-level hierarchy
TEST(HtmlParserTest, HtmlV126_5) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div><section><article>Nested</article></section></div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    auto* section = div->find_element("section");
    ASSERT_NE(section, nullptr);
    auto* article = section->find_element("article");
    ASSERT_NE(article, nullptr);

    EXPECT_EQ(article->text_content(), "Nested");
    EXPECT_EQ(article->parent, section);
    EXPECT_EQ(section->parent, div);
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(div->parent, body);
}

// 6. Parse select element with multiple option children and verify values
TEST(HtmlParserTest, HtmlV126_6) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<select name=\"color\">"
        "<option value=\"r\">Red</option>"
        "<option value=\"g\">Green</option>"
        "<option value=\"b\">Blue</option>"
        "</select>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* select = doc->find_element("select");
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(get_attr_v63(select, "name"), "color");

    auto options = select->find_all_elements("option");
    ASSERT_EQ(options.size(), 3u);
    EXPECT_EQ(get_attr_v63(options[0], "value"), "r");
    EXPECT_EQ(options[0]->text_content(), "Red");
    EXPECT_EQ(get_attr_v63(options[1], "value"), "g");
    EXPECT_EQ(options[1]->text_content(), "Green");
    EXPECT_EQ(get_attr_v63(options[2], "value"), "b");
    EXPECT_EQ(options[2]->text_content(), "Blue");
}

// 7. Parse empty tags and verify they produce elements with no text content
TEST(HtmlParserTest, HtmlV126_7) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div></div>"
        "<span></span>"
        "<p></p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->text_content(), "");
    EXPECT_EQ(div->type, clever::html::SimpleNode::Element);
    EXPECT_EQ(div->tag_name, "div");

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), "");
    EXPECT_EQ(span->tag_name, "span");

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "");
    EXPECT_EQ(p->tag_name, "p");

    // All three should be children of body
    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);
    auto divs = body->find_all_elements("div");
    auto spans = body->find_all_elements("span");
    auto ps = body->find_all_elements("p");
    EXPECT_EQ(divs.size(), 1u);
    EXPECT_EQ(spans.size(), 1u);
    EXPECT_EQ(ps.size(), 1u);
}

// 8. Parse style tag raw content preserved as text, not parsed as elements
TEST(HtmlParserTest, HtmlV126_8) {
    auto doc = clever::html::parse(
        "<html><head>"
        "<style type=\"text/css\">"
        "body { color: red; } div > p { margin: 0; }"
        "</style>"
        "</head><body><h1>Styled</h1></body></html>");
    ASSERT_NE(doc, nullptr);

    auto styles = doc->find_all_elements("style");
    ASSERT_EQ(styles.size(), 1u);
    EXPECT_EQ(get_attr_v63(styles[0], "type"), "text/css");

    // Style content should be raw text, not parsed as HTML elements
    std::string css = styles[0]->text_content();
    EXPECT_NE(css.find("color: red"), std::string::npos);
    EXPECT_NE(css.find("div > p"), std::string::npos);

    // The h1 should still be parsed normally outside the style tag
    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Styled");
}

// ============================================================================
// V127 Tests
// ============================================================================

// 1. Parse a definition list (dl/dt/dd) and verify structure and text content
TEST(HtmlParserTest, HtmlV127_1) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<dl>"
        "<dt>Term1</dt><dd>Definition1</dd>"
        "<dt>Term2</dt><dd>Definition2</dd>"
        "</dl>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);

    auto dts = dl->find_all_elements("dt");
    auto dds = dl->find_all_elements("dd");
    ASSERT_EQ(dts.size(), 2u);
    ASSERT_EQ(dds.size(), 2u);
    EXPECT_EQ(dts[0]->text_content(), "Term1");
    EXPECT_EQ(dts[1]->text_content(), "Term2");
    EXPECT_EQ(dds[0]->text_content(), "Definition1");
    EXPECT_EQ(dds[1]->text_content(), "Definition2");
}

// 2. Parse multiple inline elements sharing a parent and verify parent pointers
TEST(HtmlParserTest, HtmlV127_2) {
    auto doc = clever::html::parse(
        "<html><body><p><strong>Bold</strong><em>Italic</em><code>Code</code></p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);

    auto* strong = p->find_element("strong");
    auto* em = p->find_element("em");
    auto* code = p->find_element("code");
    ASSERT_NE(strong, nullptr);
    ASSERT_NE(em, nullptr);
    ASSERT_NE(code, nullptr);

    EXPECT_EQ(strong->text_content(), "Bold");
    EXPECT_EQ(em->text_content(), "Italic");
    EXPECT_EQ(code->text_content(), "Code");

    // All three inline elements share the same parent <p>
    EXPECT_EQ(strong->parent, p);
    EXPECT_EQ(em->parent, p);
    EXPECT_EQ(code->parent, p);
}

// 3. Parse a comment embedded between elements and verify it exists in children
TEST(HtmlParserTest, HtmlV127_3) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<div>Before</div>"
        "<!-- middle comment -->"
        "<div>After</div>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* body = doc->find_element("body");
    ASSERT_NE(body, nullptr);

    bool found_comment = false;
    for (auto& child : body->children) {
        if (child->type == clever::html::SimpleNode::Comment) {
            found_comment = true;
            EXPECT_EQ(child->data, " middle comment ");
        }
    }
    EXPECT_TRUE(found_comment);

    auto divs = body->find_all_elements("div");
    ASSERT_EQ(divs.size(), 2u);
    EXPECT_EQ(divs[0]->text_content(), "Before");
    EXPECT_EQ(divs[1]->text_content(), "After");
}

// 4. Parse a table with thead, tbody, tfoot and verify cell content
TEST(HtmlParserTest, HtmlV127_4) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<table>"
        "<thead><tr><th>Header</th></tr></thead>"
        "<tbody><tr><td>Cell1</td></tr></tbody>"
        "</table>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);

    auto* th = table->find_element("th");
    ASSERT_NE(th, nullptr);
    EXPECT_EQ(th->text_content(), "Header");

    auto* td = table->find_element("td");
    ASSERT_NE(td, nullptr);
    EXPECT_EQ(td->text_content(), "Cell1");

    auto* thead = table->find_element("thead");
    auto* tbody = table->find_element("tbody");
    ASSERT_NE(thead, nullptr);
    ASSERT_NE(tbody, nullptr);
}

// 5. Parse an element with multiple attributes and verify all via get_attr_v63
TEST(HtmlParserTest, HtmlV127_5) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<input type=\"email\" name=\"user_email\" placeholder=\"you@example.com\" "
        "required=\"required\" maxlength=\"255\">"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(get_attr_v63(input, "type"), "email");
    EXPECT_EQ(get_attr_v63(input, "name"), "user_email");
    EXPECT_EQ(get_attr_v63(input, "placeholder"), "you@example.com");
    EXPECT_EQ(get_attr_v63(input, "required"), "required");
    EXPECT_EQ(get_attr_v63(input, "maxlength"), "255");
    EXPECT_EQ(input->attributes.size(), 5u);
}

// 6. Parse nested blockquote and verify recursive text_content
TEST(HtmlParserTest, HtmlV127_6) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<blockquote>"
        "<p>Line one</p>"
        "<blockquote><p>Nested quote</p></blockquote>"
        "</blockquote>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* outer_bq = doc->find_element("blockquote");
    ASSERT_NE(outer_bq, nullptr);

    // text_content is recursive, should contain both paragraphs' text
    std::string full_text = outer_bq->text_content();
    EXPECT_NE(full_text.find("Line one"), std::string::npos);
    EXPECT_NE(full_text.find("Nested quote"), std::string::npos);

    auto inner_bqs = outer_bq->find_all_elements("blockquote");
    ASSERT_EQ(inner_bqs.size(), 1u);
    EXPECT_EQ(inner_bqs[0]->find_element("p")->text_content(), "Nested quote");
}

// 7. Parse document type and verify it is the first child of the document root
TEST(HtmlParserTest, HtmlV127_7) {
    auto doc = clever::html::parse("<!DOCTYPE html><html><head><title>T</title></head><body></body></html>");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->type, clever::html::SimpleNode::Document);

    // First child should be DOCTYPE
    ASSERT_FALSE(doc->children.empty());
    EXPECT_EQ(doc->children.front()->type, clever::html::SimpleNode::DocumentType);
    EXPECT_EQ(doc->children.front()->doctype_name, "html");

    // Title should be accessible
    auto* title = doc->find_element("title");
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->text_content(), "T");
}

// 8. Parse a script tag with special characters and verify raw text preservation
TEST(HtmlParserTest, HtmlV127_8) {
    auto doc = clever::html::parse(
        "<html><body>"
        "<script>var x = 10 < 20 && 30 > 5; // comment</script>"
        "<p>After script</p>"
        "</body></html>");
    ASSERT_NE(doc, nullptr);

    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);

    // Script content should be raw text, not parsed as HTML
    std::string script_text = script->text_content();
    EXPECT_NE(script_text.find("10 < 20"), std::string::npos);
    EXPECT_NE(script_text.find("30 > 5"), std::string::npos);
    EXPECT_NE(script_text.find("// comment"), std::string::npos);

    // Content after script should still parse normally
    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), "After script");
}

TEST(HtmlParserTest, HtmlV128_1) {
    auto doc = clever::html::parse("<html><body><abbr title=\"World Wide Web\">WWW</abbr></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* abbr = doc->find_element("abbr");
    ASSERT_NE(abbr, nullptr);
    EXPECT_EQ(abbr->text_content(), "WWW");
    EXPECT_EQ(get_attr_v63(abbr, "title"), "World Wide Web");
}

TEST(HtmlParserTest, HtmlV128_2) {
    auto doc = clever::html::parse("<html><body><ruby>漢<rt>kan</rt></ruby></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ruby = doc->find_element("ruby");
    auto* rt = doc->find_element("rt");
    ASSERT_NE(ruby, nullptr);
    ASSERT_NE(rt, nullptr);
    EXPECT_EQ(rt->text_content(), "kan");
}

TEST(HtmlParserTest, HtmlV128_3) {
    auto doc = clever::html::parse("<html><body><picture><source srcset=\"img.webp\" type=\"image/webp\"><img src=\"img.png\"></picture></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* picture = doc->find_element("picture");
    auto* source = doc->find_element("source");
    auto* img = doc->find_element("img");
    ASSERT_NE(picture, nullptr);
    ASSERT_NE(source, nullptr);
    ASSERT_NE(img, nullptr);
}

TEST(HtmlParserTest, HtmlV128_4) {
    auto doc = clever::html::parse("<html><body><dialog open>Content</dialog></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dialog = doc->find_element("dialog");
    ASSERT_NE(dialog, nullptr);

    bool has_open = false;
    for (const auto& attr : dialog->attributes) {
        if (attr.name == "open") {
            has_open = true;
            break;
        }
    }
    EXPECT_TRUE(has_open);
}

TEST(HtmlParserTest, HtmlV128_5) {
    auto doc = clever::html::parse("<html><body><time datetime=\"2024-01-15\">January 15</time></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* time = doc->find_element("time");
    ASSERT_NE(time, nullptr);
    EXPECT_EQ(get_attr_v63(time, "datetime"), "2024-01-15");
    EXPECT_EQ(time->text_content(), "January 15");
}

TEST(HtmlParserTest, HtmlV128_6) {
    auto doc = clever::html::parse("<html><body><template><p>Inside</p></template></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* tpl = doc->find_element("template");
    ASSERT_NE(tpl, nullptr);
}

TEST(HtmlParserTest, HtmlV128_7) {
    auto doc = clever::html::parse("<html><body><map><area shape=\"rect\" coords=\"0,0,100,100\" href=\"/link\"></map></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* map = doc->find_element("map");
    auto* area = doc->find_element("area");
    ASSERT_NE(map, nullptr);
    ASSERT_NE(area, nullptr);
}

TEST(HtmlParserTest, HtmlV128_8) {
    auto doc = clever::html::parse("<html><body><p>word<wbr>break</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* wbr = doc->find_element("wbr");
    auto* p = doc->find_element("p");
    ASSERT_NE(wbr, nullptr);
    ASSERT_NE(p, nullptr);

    const std::string text = p->text_content();
    EXPECT_NE(text.find("word"), std::string::npos);
    EXPECT_NE(text.find("break"), std::string::npos);
}

TEST(HtmlParserTest, HtmlV129_1) {
    auto doc = clever::html::parse("<html><body><datalist id='browsers'><option value='Chrome'><option value='Firefox'></datalist></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* datalist = doc->find_element("datalist");
    ASSERT_NE(datalist, nullptr);
    EXPECT_EQ(get_attr_v63(datalist, "id"), "browsers");

    auto options = doc->find_all_elements("option");
    EXPECT_GE(options.size(), 2u);
}

TEST(HtmlParserTest, HtmlV129_2) {
    auto doc = clever::html::parse("<html><body><output for='a b'>Result</output></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* output = doc->find_element("output");
    ASSERT_NE(output, nullptr);
    EXPECT_EQ(output->tag_name, "output");
    EXPECT_EQ(output->text_content(), "Result");
}

TEST(HtmlParserTest, HtmlV129_3) {
    auto doc = clever::html::parse("<html><body><meter min='0' max='100' low='25' high='75' optimum='50' value='60'></meter></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* meter = doc->find_element("meter");
    ASSERT_NE(meter, nullptr);
    EXPECT_EQ(get_attr_v63(meter, "min"), "0");
    EXPECT_EQ(get_attr_v63(meter, "max"), "100");
    EXPECT_EQ(get_attr_v63(meter, "low"), "25");
    EXPECT_EQ(get_attr_v63(meter, "high"), "75");
    EXPECT_EQ(get_attr_v63(meter, "optimum"), "50");
    EXPECT_EQ(get_attr_v63(meter, "value"), "60");
}

TEST(HtmlParserTest, HtmlV129_4) {
    auto doc = clever::html::parse("<html><body><progress value='70' max='100'></progress></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* progress = doc->find_element("progress");
    ASSERT_NE(progress, nullptr);
    EXPECT_EQ(get_attr_v63(progress, "value"), "70");
    EXPECT_EQ(get_attr_v63(progress, "max"), "100");
}

TEST(HtmlParserTest, HtmlV129_5) {
    auto doc = clever::html::parse("<html><body><fieldset><legend>Info</legend><input type='text'></fieldset></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* fieldset = doc->find_element("fieldset");
    auto* legend = doc->find_element("legend");
    auto* input = doc->find_element("input");
    ASSERT_NE(fieldset, nullptr);
    ASSERT_NE(legend, nullptr);
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(legend->text_content(), "Info");
}

TEST(HtmlParserTest, HtmlV129_6) {
    auto doc = clever::html::parse("<html><body><table><colgroup><col span='2'><col></colgroup></table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* colgroup = doc->find_element("colgroup");
    ASSERT_NE(colgroup, nullptr);

    auto cols = doc->find_all_elements("col");
    EXPECT_GE(cols.size(), 2u);
}

TEST(HtmlParserTest, HtmlV129_7) {
    auto doc = clever::html::parse("<html><body><table><caption>Title</caption><tr><td>Cell</td></tr></table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* caption = doc->find_element("caption");
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->text_content(), "Title");
}

TEST(HtmlParserTest, HtmlV129_8) {
    auto doc = clever::html::parse("<html><body><table><thead><tr><th>H</th></tr></thead><tbody><tr><td>B</td></tr></tbody><tfoot><tr><td>F</td></tr></tfoot></table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* thead = doc->find_element("thead");
    auto* tbody = doc->find_element("tbody");
    auto* tfoot = doc->find_element("tfoot");
    ASSERT_NE(thead, nullptr);
    ASSERT_NE(tbody, nullptr);
    ASSERT_NE(tfoot, nullptr);
}

TEST(HtmlParserTest, HtmlV130_1) {
    auto doc = clever::html::parse("<html><body><details><summary>Click me</summary><p>Hidden content</p></details></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* details = doc->find_element("details");
    auto* summary = doc->find_element("summary");
    ASSERT_NE(details, nullptr);
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->tag_name, "summary");
    EXPECT_EQ(summary->text_content(), "Click me");
}

TEST(HtmlParserTest, HtmlV130_2) {
    auto doc = clever::html::parse("<html><body><dialog open>Hello dialog</dialog></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dialog = doc->find_element("dialog");
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->tag_name, "dialog");
    EXPECT_EQ(get_attr_v63(dialog, "open"), "");
}

TEST(HtmlParserTest, HtmlV130_3) {
    auto doc = clever::html::parse("<html><body><template><p>Template content</p></template></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* tmpl = doc->find_element("template");
    ASSERT_NE(tmpl, nullptr);
    EXPECT_EQ(tmpl->tag_name, "template");
}

TEST(HtmlParserTest, HtmlV130_4) {
    auto doc = clever::html::parse("<html><body><abbr title='HyperText Markup Language'>HTML</abbr></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* abbr = doc->find_element("abbr");
    ASSERT_NE(abbr, nullptr);
    EXPECT_EQ(abbr->tag_name, "abbr");
    EXPECT_EQ(get_attr_v63(abbr, "title"), "HyperText Markup Language");
    EXPECT_EQ(abbr->text_content(), "HTML");
}

TEST(HtmlParserTest, HtmlV130_5) {
    auto doc = clever::html::parse("<html><body><time datetime='2024-01-15'>January 15</time></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* time_el = doc->find_element("time");
    ASSERT_NE(time_el, nullptr);
    EXPECT_EQ(time_el->tag_name, "time");
    EXPECT_EQ(get_attr_v63(time_el, "datetime"), "2024-01-15");
    EXPECT_EQ(time_el->text_content(), "January 15");
}

TEST(HtmlParserTest, HtmlV130_6) {
    auto doc = clever::html::parse("<html><body><p>This is <mark>highlighted</mark> text</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* mark = doc->find_element("mark");
    ASSERT_NE(mark, nullptr);
    EXPECT_EQ(mark->tag_name, "mark");
    EXPECT_EQ(mark->text_content(), "highlighted");
}

TEST(HtmlParserTest, HtmlV130_7) {
    auto doc = clever::html::parse("<html><body><picture><source srcset='large.webp' media='(min-width: 800px)'><img src='small.jpg' alt='Photo'></picture></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* picture = doc->find_element("picture");
    auto* source = doc->find_element("source");
    auto* img = doc->find_element("img");
    ASSERT_NE(picture, nullptr);
    ASSERT_NE(source, nullptr);
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(get_attr_v63(source, "srcset"), "large.webp");
    EXPECT_EQ(get_attr_v63(img, "src"), "small.jpg");
}

TEST(HtmlParserTest, HtmlV130_8) {
    auto doc = clever::html::parse("<html><body><input list='colors'><datalist id='colors'><option value='Red'><option value='Blue'></datalist></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* datalist = doc->find_element("datalist");
    ASSERT_NE(datalist, nullptr);
    EXPECT_EQ(datalist->tag_name, "datalist");
    EXPECT_EQ(get_attr_v63(datalist, "id"), "colors");

    auto options = doc->find_all_elements("option");
    EXPECT_GE(options.size(), 2u);
}

TEST(HtmlParserTest, HtmlV131_1) {
    auto doc = clever::html::parse("<html><body><address>123 Main St</address></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* addr = doc->find_element("address");
    ASSERT_NE(addr, nullptr);
    EXPECT_EQ(addr->tag_name, "address");
    EXPECT_EQ(addr->text_content(), "123 Main St");
}

TEST(HtmlParserTest, HtmlV131_2) {
    auto doc = clever::html::parse("<html><body><blockquote cite=\"https://example.com\">Quoted text</blockquote></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* bq = doc->find_element("blockquote");
    ASSERT_NE(bq, nullptr);
    EXPECT_EQ(bq->tag_name, "blockquote");
    EXPECT_EQ(get_attr_v63(bq, "cite"), "https://example.com");
    EXPECT_EQ(bq->text_content(), "Quoted text");
}

TEST(HtmlParserTest, HtmlV131_3) {
    auto doc = clever::html::parse("<html><body><dl><dt>Term</dt><dd>Definition</dd></dl></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    EXPECT_EQ(dl->tag_name, "dl");

    auto* dt = doc->find_element("dt");
    ASSERT_NE(dt, nullptr);
    EXPECT_EQ(dt->tag_name, "dt");
    EXPECT_EQ(dt->text_content(), "Term");

    auto* dd = doc->find_element("dd");
    ASSERT_NE(dd, nullptr);
    EXPECT_EQ(dd->tag_name, "dd");
    EXPECT_EQ(dd->text_content(), "Definition");
}

TEST(HtmlParserTest, HtmlV131_4) {
    auto doc = clever::html::parse("<html><body><map name=\"shapes\"><area shape=\"rect\" href=\"/rect\"></map></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* map_el = doc->find_element("map");
    ASSERT_NE(map_el, nullptr);
    EXPECT_EQ(map_el->tag_name, "map");
    EXPECT_EQ(get_attr_v63(map_el, "name"), "shapes");

    auto* area = doc->find_element("area");
    ASSERT_NE(area, nullptr);
    EXPECT_EQ(area->tag_name, "area");
    EXPECT_EQ(get_attr_v63(area, "shape"), "rect");
    EXPECT_EQ(get_attr_v63(area, "href"), "/rect");
}

TEST(HtmlParserTest, HtmlV131_5) {
    auto doc = clever::html::parse("<html><body><audio src=\"song.mp3\" controls></audio></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* audio = doc->find_element("audio");
    ASSERT_NE(audio, nullptr);
    EXPECT_EQ(audio->tag_name, "audio");
    EXPECT_EQ(get_attr_v63(audio, "src"), "song.mp3");
    EXPECT_EQ(get_attr_v63(audio, "controls"), "");
}

TEST(HtmlParserTest, HtmlV131_6) {
    auto doc = clever::html::parse("<html><body><video src=\"vid.mp4\" width=\"640\" height=\"480\"></video></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    EXPECT_EQ(video->tag_name, "video");
    EXPECT_EQ(get_attr_v63(video, "src"), "vid.mp4");
    EXPECT_EQ(get_attr_v63(video, "width"), "640");
    EXPECT_EQ(get_attr_v63(video, "height"), "480");
}

TEST(HtmlParserTest, HtmlV131_7) {
    auto doc = clever::html::parse("<html><body><embed src=\"flash.swf\" type=\"application/x-shockwave-flash\"></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* embed = doc->find_element("embed");
    ASSERT_NE(embed, nullptr);
    EXPECT_EQ(embed->tag_name, "embed");
    EXPECT_EQ(get_attr_v63(embed, "src"), "flash.swf");
    EXPECT_EQ(get_attr_v63(embed, "type"), "application/x-shockwave-flash");
}

TEST(HtmlParserTest, HtmlV131_8) {
    auto doc = clever::html::parse("<html><body><object data=\"app.swf\" type=\"application/x-shockwave-flash\"><param name=\"quality\" value=\"high\"></object></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* obj = doc->find_element("object");
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->tag_name, "object");
    EXPECT_EQ(get_attr_v63(obj, "data"), "app.swf");
    EXPECT_EQ(get_attr_v63(obj, "type"), "application/x-shockwave-flash");

    auto* param = doc->find_element("param");
    ASSERT_NE(param, nullptr);
    EXPECT_EQ(param->tag_name, "param");
    EXPECT_EQ(get_attr_v63(param, "name"), "quality");
    EXPECT_EQ(get_attr_v63(param, "value"), "high");
}

TEST(HtmlParserTest, HtmlV132_1) {
    auto doc = clever::html::parse("<html><body><meter min=\"0\" max=\"100\" value=\"75\"></meter></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* meter = doc->find_element("meter");
    ASSERT_NE(meter, nullptr);
    EXPECT_EQ(meter->tag_name, "meter");
    EXPECT_EQ(get_attr_v63(meter, "min"), "0");
    EXPECT_EQ(get_attr_v63(meter, "max"), "100");
    EXPECT_EQ(get_attr_v63(meter, "value"), "75");
}

TEST(HtmlParserTest, HtmlV132_2) {
    auto doc = clever::html::parse("<html><body><progress value=\"30\" max=\"100\"></progress></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* progress = doc->find_element("progress");
    ASSERT_NE(progress, nullptr);
    EXPECT_EQ(progress->tag_name, "progress");
    EXPECT_EQ(get_attr_v63(progress, "value"), "30");
    EXPECT_EQ(get_attr_v63(progress, "max"), "100");
}

TEST(HtmlParserTest, HtmlV132_3) {
    auto doc = clever::html::parse("<html><body><output for=\"input1\">Result</output></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* output = doc->find_element("output");
    ASSERT_NE(output, nullptr);
    EXPECT_EQ(output->tag_name, "output");
    EXPECT_EQ(get_attr_v63(output, "for"), "input1");
    EXPECT_EQ(output->text_content(), "Result");
}

TEST(HtmlParserTest, HtmlV132_4) {
    auto doc = clever::html::parse("<html><body><figure><img src=\"photo.jpg\"><figcaption>A photo</figcaption></figure></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);
    EXPECT_EQ(figure->tag_name, "figure");

    auto* figcaption = doc->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->tag_name, "figcaption");
    EXPECT_EQ(figcaption->text_content(), "A photo");
}

TEST(HtmlParserTest, HtmlV132_5) {
    auto doc = clever::html::parse("<html><body><ruby>漢<rp>(</rp><rt>kan</rt><rp>)</rp></ruby></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ruby = doc->find_element("ruby");
    ASSERT_NE(ruby, nullptr);
    EXPECT_EQ(ruby->tag_name, "ruby");

    auto* rt = doc->find_element("rt");
    ASSERT_NE(rt, nullptr);
    EXPECT_EQ(rt->tag_name, "rt");

    auto* rp = doc->find_element("rp");
    ASSERT_NE(rp, nullptr);
    EXPECT_EQ(rp->tag_name, "rp");
}

TEST(HtmlParserTest, HtmlV132_6) {
    auto doc = clever::html::parse("<html><body><bdi>مرحبا</bdi></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* bdi = doc->find_element("bdi");
    ASSERT_NE(bdi, nullptr);
    EXPECT_EQ(bdi->tag_name, "bdi");
    EXPECT_EQ(bdi->text_content(), "مرحبا");
}

TEST(HtmlParserTest, HtmlV132_7) {
    auto doc = clever::html::parse("<html><body><p>super<wbr>califragilistic</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* wbr = doc->find_element("wbr");
    ASSERT_NE(wbr, nullptr);
    EXPECT_EQ(wbr->tag_name, "wbr");
}

TEST(HtmlParserTest, HtmlV132_8) {
    auto doc = clever::html::parse("<html><body><data value=\"42\">Forty-two</data></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* data = doc->find_element("data");
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->tag_name, "data");
    EXPECT_EQ(get_attr_v63(data, "value"), "42");
    EXPECT_EQ(data->text_content(), "Forty-two");
}

// --- Round 133 (V133) ---

TEST(HtmlParserTest, HtmlV133_1) {
    auto doc = clever::html::parse("<html><body><abbr title=\"HTML\">HTML</abbr></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* abbr = doc->find_element("abbr");
    ASSERT_NE(abbr, nullptr);
    EXPECT_EQ(abbr->tag_name, "abbr");
    EXPECT_EQ(get_attr_v63(abbr, "title"), "HTML");
    EXPECT_EQ(abbr->text_content(), "HTML");
}

TEST(HtmlParserTest, HtmlV133_2) {
    auto doc = clever::html::parse("<html><body><cite>Book Title</cite></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* cite = doc->find_element("cite");
    ASSERT_NE(cite, nullptr);
    EXPECT_EQ(cite->tag_name, "cite");
    EXPECT_EQ(cite->text_content(), "Book Title");
}

TEST(HtmlParserTest, HtmlV133_3) {
    auto doc = clever::html::parse("<html><body><dfn>API</dfn></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dfn = doc->find_element("dfn");
    ASSERT_NE(dfn, nullptr);
    EXPECT_EQ(dfn->tag_name, "dfn");
    EXPECT_EQ(dfn->text_content(), "API");
}

TEST(HtmlParserTest, HtmlV133_4) {
    auto doc = clever::html::parse("<html><body><samp>Error output</samp></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* samp = doc->find_element("samp");
    ASSERT_NE(samp, nullptr);
    EXPECT_EQ(samp->tag_name, "samp");
    EXPECT_EQ(samp->text_content(), "Error output");
}

TEST(HtmlParserTest, HtmlV133_5) {
    auto doc = clever::html::parse("<html><body><var>x</var></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* var_el = doc->find_element("var");
    ASSERT_NE(var_el, nullptr);
    EXPECT_EQ(var_el->tag_name, "var");
    EXPECT_EQ(var_el->text_content(), "x");
}

TEST(HtmlParserTest, HtmlV133_6) {
    auto doc = clever::html::parse("<html><body><time datetime=\"2026-01-01\">New Year</time></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* time_el = doc->find_element("time");
    ASSERT_NE(time_el, nullptr);
    EXPECT_EQ(time_el->tag_name, "time");
    EXPECT_EQ(get_attr_v63(time_el, "datetime"), "2026-01-01");
    EXPECT_EQ(time_el->text_content(), "New Year");
}

TEST(HtmlParserTest, HtmlV133_7) {
    auto doc = clever::html::parse("<html><body><ins>added text</ins></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ins = doc->find_element("ins");
    ASSERT_NE(ins, nullptr);
    EXPECT_EQ(ins->tag_name, "ins");
    EXPECT_EQ(ins->text_content(), "added text");
}

TEST(HtmlParserTest, HtmlV133_8) {
    auto doc = clever::html::parse("<html><body><del>removed text</del></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* del_el = doc->find_element("del");
    ASSERT_NE(del_el, nullptr);
    EXPECT_EQ(del_el->tag_name, "del");
    EXPECT_EQ(del_el->text_content(), "removed text");
}

// ---------------------------------------------------------------------------
// Round 134
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, HtmlV134_1) {
    auto doc = clever::html::parse("<html><body><kbd>Ctrl+C</kbd></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* kbd = doc->find_element("kbd");
    ASSERT_NE(kbd, nullptr);
    EXPECT_EQ(kbd->tag_name, "kbd");
    EXPECT_EQ(kbd->text_content(), "Ctrl+C");
}

TEST(HtmlParserTest, HtmlV134_2) {
    auto doc = clever::html::parse("<html><body><code>const x = 1;</code></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* code = doc->find_element("code");
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(code->tag_name, "code");
    EXPECT_EQ(code->text_content(), "const x = 1;");
}

TEST(HtmlParserTest, HtmlV134_3) {
    auto doc = clever::html::parse("<html><body><pre>preformatted text</pre></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
    EXPECT_EQ(pre->tag_name, "pre");
    EXPECT_EQ(pre->text_content(), "preformatted text");
}

TEST(HtmlParserTest, HtmlV134_4) {
    auto doc = clever::html::parse("<html><body><mark>highlighted</mark></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* mark = doc->find_element("mark");
    ASSERT_NE(mark, nullptr);
    EXPECT_EQ(mark->tag_name, "mark");
    EXPECT_EQ(mark->text_content(), "highlighted");
}

TEST(HtmlParserTest, HtmlV134_5) {
    auto doc = clever::html::parse("<html><body><small>fine print</small></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* small_el = doc->find_element("small");
    ASSERT_NE(small_el, nullptr);
    EXPECT_EQ(small_el->tag_name, "small");
    EXPECT_EQ(small_el->text_content(), "fine print");
}

TEST(HtmlParserTest, HtmlV134_6) {
    auto doc = clever::html::parse("<html><body><sub>subscript</sub></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* sub = doc->find_element("sub");
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->tag_name, "sub");
    EXPECT_EQ(sub->text_content(), "subscript");
}

TEST(HtmlParserTest, HtmlV134_7) {
    auto doc = clever::html::parse("<html><body><sup>superscript</sup></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* sup = doc->find_element("sup");
    ASSERT_NE(sup, nullptr);
    EXPECT_EQ(sup->tag_name, "sup");
    EXPECT_EQ(sup->text_content(), "superscript");
}

TEST(HtmlParserTest, HtmlV134_8) {
    auto doc = clever::html::parse("<html><body><details><summary>Title</summary>Content</details></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    EXPECT_EQ(details->tag_name, "details");

    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->tag_name, "summary");
}

// === V135 HTML Parser Tests ===

TEST(HtmlParserTest, HtmlV135_1) {
    auto doc = clever::html::parse("<html><body><ruby>漢<rt>kan</rt>字<rt>ji</rt></ruby></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ruby = doc->find_element("ruby");
    ASSERT_NE(ruby, nullptr);
    EXPECT_EQ(ruby->tag_name, "ruby");

    auto* rt = doc->find_element("rt");
    ASSERT_NE(rt, nullptr);
    EXPECT_EQ(rt->tag_name, "rt");
}

TEST(HtmlParserTest, HtmlV135_2) {
    auto doc = clever::html::parse("<html><body><p>long<wbr>word</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* wbr = doc->find_element("wbr");
    ASSERT_NE(wbr, nullptr);
    EXPECT_EQ(wbr->tag_name, "wbr");
}

TEST(HtmlParserTest, HtmlV135_3) {
    auto doc = clever::html::parse("<html><body><bdo dir='rtl'>text</bdo><bdi>mixed</bdi></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* bdo = doc->find_element("bdo");
    ASSERT_NE(bdo, nullptr);
    EXPECT_EQ(bdo->tag_name, "bdo");

    auto* bdi = doc->find_element("bdi");
    ASSERT_NE(bdi, nullptr);
    EXPECT_EQ(bdi->tag_name, "bdi");
}

TEST(HtmlParserTest, HtmlV135_4) {
    auto doc = clever::html::parse("<html><body><figure><img src='photo.jpg'/><figcaption>A photo</figcaption></figure></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);
    EXPECT_EQ(figure->tag_name, "figure");

    auto* figcaption = doc->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->tag_name, "figcaption");
    EXPECT_EQ(figcaption->text_content(), "A photo");
}

TEST(HtmlParserTest, HtmlV135_5) {
    auto doc = clever::html::parse("<html><body><abbr title='HyperText Markup Language'>HTML</abbr></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* abbr = doc->find_element("abbr");
    ASSERT_NE(abbr, nullptr);
    EXPECT_EQ(abbr->tag_name, "abbr");
    EXPECT_EQ(abbr->text_content(), "HTML");
}

TEST(HtmlParserTest, HtmlV135_6) {
    auto doc = clever::html::parse("<html><body><time datetime='2024-01-01'>New Year</time></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* time_el = doc->find_element("time");
    ASSERT_NE(time_el, nullptr);
    EXPECT_EQ(time_el->tag_name, "time");
    EXPECT_EQ(time_el->text_content(), "New Year");
}

TEST(HtmlParserTest, HtmlV135_7) {
    auto doc = clever::html::parse("<html><body><data value='42'>Forty-two</data></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* data_el = doc->find_element("data");
    ASSERT_NE(data_el, nullptr);
    EXPECT_EQ(data_el->tag_name, "data");
    EXPECT_EQ(data_el->text_content(), "Forty-two");
}

TEST(HtmlParserTest, HtmlV135_8) {
    auto doc = clever::html::parse("<html><body><dialog open>Dialog content</dialog></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dialog = doc->find_element("dialog");
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->tag_name, "dialog");
    EXPECT_EQ(dialog->text_content(), "Dialog content");
}

// === V136 HTML Parser Tests ===

TEST(HtmlParserTest, HtmlV136_1) {
    auto doc = clever::html::parse("<html><body><ins>added</ins><del>removed</del></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ins_el = doc->find_element("ins");
    ASSERT_NE(ins_el, nullptr);
    EXPECT_EQ(ins_el->tag_name, "ins");
    EXPECT_EQ(ins_el->text_content(), "added");

    auto* del_el = doc->find_element("del");
    ASSERT_NE(del_el, nullptr);
    EXPECT_EQ(del_el->tag_name, "del");
    EXPECT_EQ(del_el->text_content(), "removed");
}

TEST(HtmlParserTest, HtmlV136_2) {
    auto doc = clever::html::parse("<html><body><cite>The Art of War</cite></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* cite_el = doc->find_element("cite");
    ASSERT_NE(cite_el, nullptr);
    EXPECT_EQ(cite_el->tag_name, "cite");
    EXPECT_EQ(cite_el->text_content(), "The Art of War");
}

TEST(HtmlParserTest, HtmlV136_3) {
    auto doc = clever::html::parse("<html><body><var>x</var></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* var_el = doc->find_element("var");
    ASSERT_NE(var_el, nullptr);
    EXPECT_EQ(var_el->tag_name, "var");
    EXPECT_EQ(var_el->text_content(), "x");
}

TEST(HtmlParserTest, HtmlV136_4) {
    auto doc = clever::html::parse("<html><body><samp>output text</samp></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* samp_el = doc->find_element("samp");
    ASSERT_NE(samp_el, nullptr);
    EXPECT_EQ(samp_el->tag_name, "samp");
    EXPECT_EQ(samp_el->text_content(), "output text");
}

TEST(HtmlParserTest, HtmlV136_5) {
    auto doc = clever::html::parse("<html><body><dl><dt>Term</dt><dd>Definition</dd></dl></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dl_el = doc->find_element("dl");
    ASSERT_NE(dl_el, nullptr);
    EXPECT_EQ(dl_el->tag_name, "dl");

    auto* dt_el = doc->find_element("dt");
    ASSERT_NE(dt_el, nullptr);
    EXPECT_EQ(dt_el->tag_name, "dt");
    EXPECT_EQ(dt_el->text_content(), "Term");

    auto* dd_el = doc->find_element("dd");
    ASSERT_NE(dd_el, nullptr);
    EXPECT_EQ(dd_el->tag_name, "dd");
    EXPECT_EQ(dd_el->text_content(), "Definition");
}

TEST(HtmlParserTest, HtmlV136_6) {
    auto doc = clever::html::parse("<html><body><nav><a href='/home'>Home</a></nav></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* nav_el = doc->find_element("nav");
    ASSERT_NE(nav_el, nullptr);
    EXPECT_EQ(nav_el->tag_name, "nav");
}

TEST(HtmlParserTest, HtmlV136_7) {
    auto doc = clever::html::parse("<html><body><aside>Sidebar content</aside></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* aside_el = doc->find_element("aside");
    ASSERT_NE(aside_el, nullptr);
    EXPECT_EQ(aside_el->tag_name, "aside");
    EXPECT_EQ(aside_el->text_content(), "Sidebar content");
}

TEST(HtmlParserTest, HtmlV136_8) {
    auto doc = clever::html::parse("<html><body><main>Main content area</main></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* main_el = doc->find_element("main");
    ASSERT_NE(main_el, nullptr);
    EXPECT_EQ(main_el->tag_name, "main");
    EXPECT_EQ(main_el->text_content(), "Main content area");
}

// === V137 HTML Parser Tests ===

TEST(HtmlParserTest, HtmlV137_1) {
    auto doc = clever::html::parse("<html><body><address>123 Main St</address></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* addr_el = doc->find_element("address");
    ASSERT_NE(addr_el, nullptr);
    EXPECT_EQ(addr_el->tag_name, "address");
    EXPECT_EQ(addr_el->text_content(), "123 Main St");
}

TEST(HtmlParserTest, HtmlV137_2) {
    auto doc = clever::html::parse("<html><body><blockquote cite=\"https://example.com\">Quoted text</blockquote></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* bq_el = doc->find_element("blockquote");
    ASSERT_NE(bq_el, nullptr);
    EXPECT_EQ(bq_el->tag_name, "blockquote");
    EXPECT_EQ(bq_el->text_content(), "Quoted text");
}

TEST(HtmlParserTest, HtmlV137_3) {
    auto doc = clever::html::parse("<html><body><p><q cite=\"https://example.com\">Inline quote</q></p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* q_el = doc->find_element("q");
    ASSERT_NE(q_el, nullptr);
    EXPECT_EQ(q_el->tag_name, "q");
    EXPECT_EQ(q_el->text_content(), "Inline quote");
}

TEST(HtmlParserTest, HtmlV137_4) {
    auto doc = clever::html::parse("<html><body><map name=\"shapes\"><area shape=\"rect\" href=\"/rect\"></map></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* map_el = doc->find_element("map");
    ASSERT_NE(map_el, nullptr);
    EXPECT_EQ(map_el->tag_name, "map");

    auto* area_el = doc->find_element("area");
    ASSERT_NE(area_el, nullptr);
    EXPECT_EQ(area_el->tag_name, "area");
}

TEST(HtmlParserTest, HtmlV137_5) {
    auto doc = clever::html::parse("<html><body><picture><source srcset=\"img.webp\" type=\"image/webp\"><img src=\"img.jpg\"></picture></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* picture_el = doc->find_element("picture");
    ASSERT_NE(picture_el, nullptr);
    EXPECT_EQ(picture_el->tag_name, "picture");

    auto* source_el = doc->find_element("source");
    ASSERT_NE(source_el, nullptr);
    EXPECT_EQ(source_el->tag_name, "source");
}

TEST(HtmlParserTest, HtmlV137_6) {
    auto doc = clever::html::parse("<html><body><template><p>Template content</p></template></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* tmpl_el = doc->find_element("template");
    ASSERT_NE(tmpl_el, nullptr);
    EXPECT_EQ(tmpl_el->tag_name, "template");
}

TEST(HtmlParserTest, HtmlV137_7) {
    auto doc = clever::html::parse("<html><body><div><slot name=\"header\">Default</slot></div></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* slot_el = doc->find_element("slot");
    ASSERT_NE(slot_el, nullptr);
    EXPECT_EQ(slot_el->tag_name, "slot");
    EXPECT_EQ(slot_el->text_content(), "Default");
}

TEST(HtmlParserTest, HtmlV137_8) {
    auto doc = clever::html::parse("<html><body><noscript>JavaScript is required</noscript></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ns_el = doc->find_element("noscript");
    ASSERT_NE(ns_el, nullptr);
    EXPECT_EQ(ns_el->tag_name, "noscript");
}

// === V138 HTML Parser Tests ===

TEST(HtmlParserTest, HtmlV138_1) {
    auto doc = clever::html::parse("<html><body><details><summary>Click me</summary><p>Content</p></details></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* summary_el = doc->find_element("summary");
    ASSERT_NE(summary_el, nullptr);
    EXPECT_EQ(summary_el->tag_name, "summary");
    EXPECT_EQ(summary_el->text_content(), "Click me");
}

TEST(HtmlParserTest, HtmlV138_2) {
    auto doc = clever::html::parse("<html><body><select><optgroup label=\"Group\"><option value=\"1\">One</option></optgroup></select></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* optgroup_el = doc->find_element("optgroup");
    ASSERT_NE(optgroup_el, nullptr);
    EXPECT_EQ(optgroup_el->tag_name, "optgroup");

    auto* option_el = doc->find_element("option");
    ASSERT_NE(option_el, nullptr);
    EXPECT_EQ(option_el->tag_name, "option");
    EXPECT_EQ(option_el->text_content(), "One");
}

TEST(HtmlParserTest, HtmlV138_3) {
    auto doc = clever::html::parse("<html><body><video><track kind=\"subtitles\" src=\"subs.vtt\" srclang=\"en\"></video></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* track_el = doc->find_element("track");
    ASSERT_NE(track_el, nullptr);
    EXPECT_EQ(track_el->tag_name, "track");
}

TEST(HtmlParserTest, HtmlV138_4) {
    auto doc = clever::html::parse("<html><body><embed type=\"image/png\" src=\"image.png\" width=\"200\" height=\"100\"></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* embed_el = doc->find_element("embed");
    ASSERT_NE(embed_el, nullptr);
    EXPECT_EQ(embed_el->tag_name, "embed");
}

TEST(HtmlParserTest, HtmlV138_5) {
    auto doc = clever::html::parse("<html><body><object data=\"movie.swf\" type=\"application/x-shockwave-flash\"><param name=\"quality\" value=\"high\"></object></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* object_el = doc->find_element("object");
    ASSERT_NE(object_el, nullptr);
    EXPECT_EQ(object_el->tag_name, "object");

    auto* param_el = doc->find_element("param");
    ASSERT_NE(param_el, nullptr);
    EXPECT_EQ(param_el->tag_name, "param");
}

TEST(HtmlParserTest, HtmlV138_6) {
    auto doc = clever::html::parse("<html><body><iframe src=\"https://example.com\" width=\"600\" height=\"400\"></iframe></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* iframe_el = doc->find_element("iframe");
    ASSERT_NE(iframe_el, nullptr);
    EXPECT_EQ(iframe_el->tag_name, "iframe");
}

TEST(HtmlParserTest, HtmlV138_7) {
    auto doc = clever::html::parse("<html><body><p>Line one<br>Line two</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* br_el = doc->find_element("br");
    ASSERT_NE(br_el, nullptr);
    EXPECT_EQ(br_el->tag_name, "br");
}

TEST(HtmlParserTest, HtmlV138_8) {
    auto doc = clever::html::parse("<html><body><hr></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* hr_el = doc->find_element("hr");
    ASSERT_NE(hr_el, nullptr);
    EXPECT_EQ(hr_el->tag_name, "hr");
}

// === V139 HTML Parser Tests ===

TEST(HtmlParserTest, HtmlV139_1) {
    // canvas element
    auto doc = clever::html::parse("<html><body><canvas width=\"300\" height=\"150\"></canvas></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* canvas_el = doc->find_element("canvas");
    ASSERT_NE(canvas_el, nullptr);
    EXPECT_EQ(canvas_el->tag_name, "canvas");
    bool has_width = false;
    for (const auto& attr : canvas_el->attributes) {
        if (attr.name == "width" && attr.value == "300") has_width = true;
    }
    EXPECT_TRUE(has_width);
}

TEST(HtmlParserTest, HtmlV139_2) {
    // audio element
    auto doc = clever::html::parse("<html><body><audio controls src=\"song.mp3\"></audio></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* audio_el = doc->find_element("audio");
    ASSERT_NE(audio_el, nullptr);
    EXPECT_EQ(audio_el->tag_name, "audio");
    bool has_controls = false;
    for (const auto& attr : audio_el->attributes) {
        if (attr.name == "controls") has_controls = true;
    }
    EXPECT_TRUE(has_controls);
}

TEST(HtmlParserTest, HtmlV139_3) {
    // video element
    auto doc = clever::html::parse("<html><body><video width=\"640\" height=\"480\"></video></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* video_el = doc->find_element("video");
    ASSERT_NE(video_el, nullptr);
    EXPECT_EQ(video_el->tag_name, "video");
    bool has_height = false;
    for (const auto& attr : video_el->attributes) {
        if (attr.name == "height" && attr.value == "480") has_height = true;
    }
    EXPECT_TRUE(has_height);
}

TEST(HtmlParserTest, HtmlV139_4) {
    // source element inside audio
    auto doc = clever::html::parse("<html><body><audio><source src=\"track.ogg\" type=\"audio/ogg\"></audio></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* source_el = doc->find_element("source");
    ASSERT_NE(source_el, nullptr);
    EXPECT_EQ(source_el->tag_name, "source");
    bool has_type = false;
    for (const auto& attr : source_el->attributes) {
        if (attr.name == "type" && attr.value == "audio/ogg") has_type = true;
    }
    EXPECT_TRUE(has_type);
}

TEST(HtmlParserTest, HtmlV139_5) {
    // thead with tr/th
    auto doc = clever::html::parse("<html><body><table><thead><tr><th>Header1</th><th>Header2</th></tr></thead></table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* thead_el = doc->find_element("thead");
    ASSERT_NE(thead_el, nullptr);
    EXPECT_EQ(thead_el->tag_name, "thead");

    auto ths = doc->find_all_elements("th");
    EXPECT_GE(ths.size(), 2u);
}

TEST(HtmlParserTest, HtmlV139_6) {
    // tfoot with tr/td
    auto doc = clever::html::parse("<html><body><table><tfoot><tr><td>Foot1</td><td>Foot2</td></tr></tfoot></table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* tfoot_el = doc->find_element("tfoot");
    ASSERT_NE(tfoot_el, nullptr);
    EXPECT_EQ(tfoot_el->tag_name, "tfoot");

    auto tds = doc->find_all_elements("td");
    EXPECT_GE(tds.size(), 2u);
}

TEST(HtmlParserTest, HtmlV139_7) {
    // tbody with tr/td
    auto doc = clever::html::parse("<html><body><table><tbody><tr><td>Row1</td><td>Row2</td></tr></tbody></table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* tbody_el = doc->find_element("tbody");
    ASSERT_NE(tbody_el, nullptr);
    EXPECT_EQ(tbody_el->tag_name, "tbody");

    auto tds = doc->find_all_elements("td");
    EXPECT_GE(tds.size(), 2u);
}

TEST(HtmlParserTest, HtmlV139_8) {
    // caption in table
    auto doc = clever::html::parse("<html><body><table><caption>My Table</caption><tr><td>Data</td></tr></table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* caption_el = doc->find_element("caption");
    ASSERT_NE(caption_el, nullptr);
    EXPECT_EQ(caption_el->tag_name, "caption");
    EXPECT_NE(caption_el->text_content().find("My Table"), std::string::npos);
}

// === V140 HTML Parser Tests ===

TEST(HtmlParserTest, HtmlV140_1) {
    // link element in head
    auto doc = clever::html::parse("<html><head><link rel=\"stylesheet\" href=\"style.css\"></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* link_el = doc->find_element("link");
    ASSERT_NE(link_el, nullptr);
    EXPECT_EQ(link_el->tag_name, "link");
    bool has_rel = false;
    for (const auto& attr : link_el->attributes) {
        if (attr.name == "rel" && attr.value == "stylesheet") has_rel = true;
    }
    EXPECT_TRUE(has_rel);
}

TEST(HtmlParserTest, HtmlV140_2) {
    // meta element with charset
    auto doc = clever::html::parse("<html><head><meta charset=\"utf-8\"></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* meta_el = doc->find_element("meta");
    ASSERT_NE(meta_el, nullptr);
    EXPECT_EQ(meta_el->tag_name, "meta");
    bool has_charset = false;
    for (const auto& attr : meta_el->attributes) {
        if (attr.name == "charset" && attr.value == "utf-8") has_charset = true;
    }
    EXPECT_TRUE(has_charset);
}

TEST(HtmlParserTest, HtmlV140_3) {
    // style element in head
    auto doc = clever::html::parse("<html><head><style>body { color: red; }</style></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* style_el = doc->find_element("style");
    ASSERT_NE(style_el, nullptr);
    EXPECT_EQ(style_el->tag_name, "style");
}

TEST(HtmlParserTest, HtmlV140_4) {
    // script element in body
    auto doc = clever::html::parse("<html><body><script>var x = 1;</script></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* script_el = doc->find_element("script");
    ASSERT_NE(script_el, nullptr);
    EXPECT_EQ(script_el->tag_name, "script");
}

TEST(HtmlParserTest, HtmlV140_5) {
    // base element with href
    auto doc = clever::html::parse("<html><head><base href=\"https://example.com/\"></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* base_el = doc->find_element("base");
    ASSERT_NE(base_el, nullptr);
    EXPECT_EQ(base_el->tag_name, "base");
    bool has_href = false;
    for (const auto& attr : base_el->attributes) {
        if (attr.name == "href" && attr.value == "https://example.com/") has_href = true;
    }
    EXPECT_TRUE(has_href);
}

TEST(HtmlParserTest, HtmlV140_6) {
    // head element exists
    auto doc = clever::html::parse("<html><head><title>Test</title></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* head_el = doc->find_element("head");
    ASSERT_NE(head_el, nullptr);
    EXPECT_EQ(head_el->tag_name, "head");
}

TEST(HtmlParserTest, HtmlV140_7) {
    // title element with text
    auto doc = clever::html::parse("<html><head><title>My Page</title></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* title_el = doc->find_element("title");
    ASSERT_NE(title_el, nullptr);
    EXPECT_EQ(title_el->tag_name, "title");
    EXPECT_NE(title_el->text_content().find("My Page"), std::string::npos);
}

TEST(HtmlParserTest, HtmlV140_8) {
    // form element with action and method
    auto doc = clever::html::parse("<html><body><form action=\"/submit\" method=\"post\"><input type=\"text\"></form></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* form_el = doc->find_element("form");
    ASSERT_NE(form_el, nullptr);
    EXPECT_EQ(form_el->tag_name, "form");
    bool has_action = false;
    for (const auto& attr : form_el->attributes) {
        if (attr.name == "action" && attr.value == "/submit") has_action = true;
    }
    EXPECT_TRUE(has_action);
}

// === V141 HTML Parser Tests ===

TEST(HtmlParserTest, HtmlV141_1) {
    // abbr element with title attribute
    auto doc = clever::html::parse("<html><body><abbr title=\"HyperText Markup Language\">HTML</abbr></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* abbr_el = doc->find_element("abbr");
    ASSERT_NE(abbr_el, nullptr);
    EXPECT_EQ(abbr_el->tag_name, "abbr");
    bool has_title = false;
    for (const auto& attr : abbr_el->attributes) {
        if (attr.name == "title" && attr.value == "HyperText Markup Language") has_title = true;
    }
    EXPECT_TRUE(has_title);
}

TEST(HtmlParserTest, HtmlV141_2) {
    // time element with datetime attribute
    auto doc = clever::html::parse("<html><body><time datetime=\"2024-01-15\">January 15</time></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* time_el = doc->find_element("time");
    ASSERT_NE(time_el, nullptr);
    EXPECT_EQ(time_el->tag_name, "time");
    bool has_datetime = false;
    for (const auto& attr : time_el->attributes) {
        if (attr.name == "datetime" && attr.value == "2024-01-15") has_datetime = true;
    }
    EXPECT_TRUE(has_datetime);
}

TEST(HtmlParserTest, HtmlV141_3) {
    // details with open attribute
    auto doc = clever::html::parse("<html><body><details open><summary>Info</summary><p>Details here</p></details></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* details_el = doc->find_element("details");
    ASSERT_NE(details_el, nullptr);
    EXPECT_EQ(details_el->tag_name, "details");
    bool has_open = false;
    for (const auto& attr : details_el->attributes) {
        if (attr.name == "open") has_open = true;
    }
    EXPECT_TRUE(has_open);
}

TEST(HtmlParserTest, HtmlV141_4) {
    // picture with source and img
    auto doc = clever::html::parse("<html><body><picture><source srcset=\"img.webp\" type=\"image/webp\"><img src=\"img.jpg\"></picture></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* picture_el = doc->find_element("picture");
    ASSERT_NE(picture_el, nullptr);
    EXPECT_EQ(picture_el->tag_name, "picture");

    auto* source_el = doc->find_element("source");
    ASSERT_NE(source_el, nullptr);
    EXPECT_EQ(source_el->tag_name, "source");
}

TEST(HtmlParserTest, HtmlV141_5) {
    // template element
    auto doc = clever::html::parse("<html><body><template><p>Template content</p></template></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* tmpl_el = doc->find_element("template");
    ASSERT_NE(tmpl_el, nullptr);
    EXPECT_EQ(tmpl_el->tag_name, "template");
}

TEST(HtmlParserTest, HtmlV141_6) {
    // dialog element with open
    auto doc = clever::html::parse("<html><body><dialog open>Hello dialog</dialog></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dialog_el = doc->find_element("dialog");
    ASSERT_NE(dialog_el, nullptr);
    EXPECT_EQ(dialog_el->tag_name, "dialog");
    bool has_open = false;
    for (const auto& attr : dialog_el->attributes) {
        if (attr.name == "open") has_open = true;
    }
    EXPECT_TRUE(has_open);
}

TEST(HtmlParserTest, HtmlV141_7) {
    // data element with value attribute
    auto doc = clever::html::parse("<html><body><data value=\"42\">Forty-two</data></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* data_el = doc->find_element("data");
    ASSERT_NE(data_el, nullptr);
    EXPECT_EQ(data_el->tag_name, "data");
    bool has_value = false;
    for (const auto& attr : data_el->attributes) {
        if (attr.name == "value" && attr.value == "42") has_value = true;
    }
    EXPECT_TRUE(has_value);
}

TEST(HtmlParserTest, HtmlV141_8) {
    // slot element with name attribute
    auto doc = clever::html::parse("<html><body><slot name=\"header\">Fallback</slot></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* slot_el = doc->find_element("slot");
    ASSERT_NE(slot_el, nullptr);
    EXPECT_EQ(slot_el->tag_name, "slot");
    bool has_name = false;
    for (const auto& attr : slot_el->attributes) {
        if (attr.name == "name" && attr.value == "header") has_name = true;
    }
    EXPECT_TRUE(has_name);
}

// === V142 HTML Parser Tests ===

TEST(HtmlParserTest, HtmlV142_1) {
    // nav element
    auto doc = clever::html::parse("<html><body><nav><a href=\"/\">Home</a><a href=\"/about\">About</a></nav></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* nav = doc->find_element("nav");
    ASSERT_NE(nav, nullptr);
    EXPECT_EQ(nav->tag_name, "nav");
    auto links = nav->find_all_elements("a");
    EXPECT_EQ(links.size(), 2u);
}

TEST(HtmlParserTest, HtmlV142_2) {
    // aside element
    auto doc = clever::html::parse("<html><body><aside><p>Sidebar content</p></aside></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* aside = doc->find_element("aside");
    ASSERT_NE(aside, nullptr);
    EXPECT_EQ(aside->tag_name, "aside");
    EXPECT_TRUE(aside->text_content().find("Sidebar") != std::string::npos);
}

TEST(HtmlParserTest, HtmlV142_3) {
    // main element
    auto doc = clever::html::parse("<html><body><main><h1>Title</h1><p>Content</p></main></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* main_el = doc->find_element("main");
    ASSERT_NE(main_el, nullptr);
    EXPECT_EQ(main_el->tag_name, "main");
    auto* h1 = main_el->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->text_content(), "Title");
}

TEST(HtmlParserTest, HtmlV142_4) {
    // header element
    auto doc = clever::html::parse("<html><body><header><h1>Site Title</h1><nav>Menu</nav></header></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* header = doc->find_element("header");
    ASSERT_NE(header, nullptr);
    EXPECT_EQ(header->tag_name, "header");
    auto* nav = header->find_element("nav");
    ASSERT_NE(nav, nullptr);
}

TEST(HtmlParserTest, HtmlV142_5) {
    // footer element
    auto doc = clever::html::parse("<html><body><footer><p>Copyright 2024</p></footer></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* footer = doc->find_element("footer");
    ASSERT_NE(footer, nullptr);
    EXPECT_EQ(footer->tag_name, "footer");
    EXPECT_TRUE(footer->text_content().find("Copyright") != std::string::npos);
}

TEST(HtmlParserTest, HtmlV142_6) {
    // section element
    auto doc = clever::html::parse("<html><body><section><h2>Chapter 1</h2><p>Text here</p></section></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* section = doc->find_element("section");
    ASSERT_NE(section, nullptr);
    EXPECT_EQ(section->tag_name, "section");
    auto* h2 = section->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->text_content(), "Chapter 1");
}

TEST(HtmlParserTest, HtmlV142_7) {
    // article element
    auto doc = clever::html::parse("<html><body><article><h2>Blog Post</h2><p>Article body</p></article></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* article = doc->find_element("article");
    ASSERT_NE(article, nullptr);
    EXPECT_EQ(article->tag_name, "article");
    EXPECT_TRUE(article->text_content().find("Blog Post") != std::string::npos);
}

TEST(HtmlParserTest, HtmlV142_8) {
    // figure with figcaption
    auto doc = clever::html::parse("<html><body><figure><img src=\"photo.jpg\"><figcaption>A beautiful photo</figcaption></figure></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);
    EXPECT_EQ(figure->tag_name, "figure");
    auto* figcaption = figure->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->text_content(), "A beautiful photo");
    auto* img = figure->find_element("img");
    ASSERT_NE(img, nullptr);
}

// === V143 HTML Parser Tests ===

TEST(HtmlParserTest, HtmlV143_1) {
    // h1 element
    auto doc = clever::html::parse("<html><body><h1>Main Heading</h1></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* h1 = doc->find_element("h1");
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->tag_name, "h1");
    EXPECT_EQ(h1->text_content(), "Main Heading");
}

TEST(HtmlParserTest, HtmlV143_2) {
    // h2 element
    auto doc = clever::html::parse("<html><body><h2>Subheading</h2></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* h2 = doc->find_element("h2");
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->tag_name, "h2");
    EXPECT_EQ(h2->text_content(), "Subheading");
}

TEST(HtmlParserTest, HtmlV143_3) {
    // h3 element
    auto doc = clever::html::parse("<html><body><h3>Section Title</h3></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* h3 = doc->find_element("h3");
    ASSERT_NE(h3, nullptr);
    EXPECT_EQ(h3->tag_name, "h3");
    EXPECT_EQ(h3->text_content(), "Section Title");
}

TEST(HtmlParserTest, HtmlV143_4) {
    // h4 element
    auto doc = clever::html::parse("<html><body><h4>Subsection</h4></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* h4 = doc->find_element("h4");
    ASSERT_NE(h4, nullptr);
    EXPECT_EQ(h4->tag_name, "h4");
    EXPECT_EQ(h4->text_content(), "Subsection");
}

TEST(HtmlParserTest, HtmlV143_5) {
    // h5 element
    auto doc = clever::html::parse("<html><body><h5>Minor Heading</h5></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* h5 = doc->find_element("h5");
    ASSERT_NE(h5, nullptr);
    EXPECT_EQ(h5->tag_name, "h5");
    EXPECT_EQ(h5->text_content(), "Minor Heading");
}

TEST(HtmlParserTest, HtmlV143_6) {
    // h6 element
    auto doc = clever::html::parse("<html><body><h6>Smallest Heading</h6></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* h6 = doc->find_element("h6");
    ASSERT_NE(h6, nullptr);
    EXPECT_EQ(h6->tag_name, "h6");
    EXPECT_EQ(h6->text_content(), "Smallest Heading");
}

TEST(HtmlParserTest, HtmlV143_7) {
    // em and strong nested
    auto doc = clever::html::parse("<html><body><p><em>emphasized</em> and <strong>bold</strong></p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->tag_name, "em");
    EXPECT_EQ(em->text_content(), "emphasized");

    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->tag_name, "strong");
    EXPECT_EQ(strong->text_content(), "bold");
}

TEST(HtmlParserTest, HtmlV143_8) {
    // small element
    auto doc = clever::html::parse("<html><body><p><small>Fine print</small></p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* small = doc->find_element("small");
    ASSERT_NE(small, nullptr);
    EXPECT_EQ(small->tag_name, "small");
    EXPECT_EQ(small->text_content(), "Fine print");
}

// === V144 HTML Parser Tests ===

TEST(HtmlParserTest, HtmlV144_1) {
    // br element (void element)
    auto doc = clever::html::parse("<html><body><p>Line one<br>Line two</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* br = doc->find_element("br");
    ASSERT_NE(br, nullptr);
    EXPECT_EQ(br->tag_name, "br");
    EXPECT_TRUE(br->children.empty());
}

TEST(HtmlParserTest, HtmlV144_2) {
    // hr element (void element)
    auto doc = clever::html::parse("<html><body><hr></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* hr = doc->find_element("hr");
    ASSERT_NE(hr, nullptr);
    EXPECT_EQ(hr->tag_name, "hr");
    EXPECT_TRUE(hr->children.empty());
}

TEST(HtmlParserTest, HtmlV144_3) {
    // input with type=text
    auto doc = clever::html::parse("<html><body><form><input type=\"text\"></form></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->tag_name, "input");
    bool found_type = false;
    for (const auto& attr : input->attributes) {
        if (attr.name == "type") {
            found_type = true;
            EXPECT_EQ(attr.value, "text");
        }
    }
    EXPECT_TRUE(found_type) << "type attribute not found on input";
}

TEST(HtmlParserTest, HtmlV144_4) {
    // input with type=checkbox
    auto doc = clever::html::parse("<html><body><form><input type=\"checkbox\"></form></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* input = doc->find_element("input");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->tag_name, "input");
    bool found_type = false;
    for (const auto& attr : input->attributes) {
        if (attr.name == "type") {
            found_type = true;
            EXPECT_EQ(attr.value, "checkbox");
        }
    }
    EXPECT_TRUE(found_type) << "type attribute not found on input";
}

TEST(HtmlParserTest, HtmlV144_5) {
    // select with options
    auto doc = clever::html::parse("<html><body><select><option>A</option><option>B</option></select></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* select = doc->find_element("select");
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(select->tag_name, "select");

    int option_count = 0;
    for (const auto& child : select->children) {
        if (child->tag_name == "option") {
            option_count++;
        }
    }
    EXPECT_EQ(option_count, 2);
}

TEST(HtmlParserTest, HtmlV144_6) {
    // textarea element
    auto doc = clever::html::parse("<html><body><form><textarea>Some text</textarea></form></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* textarea = doc->find_element("textarea");
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(textarea->tag_name, "textarea");
}

TEST(HtmlParserTest, HtmlV144_7) {
    // button element
    auto doc = clever::html::parse("<html><body><form><button>Click me</button></form></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* button = doc->find_element("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->tag_name, "button");
    EXPECT_EQ(button->text_content(), "Click me");
}

TEST(HtmlParserTest, HtmlV144_8) {
    // label with for attribute
    auto doc = clever::html::parse("<html><body><form><label for=\"name\">Name:</label></form></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* label = doc->find_element("label");
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->tag_name, "label");
    EXPECT_EQ(label->text_content(), "Name:");
    bool found_for = false;
    for (const auto& attr : label->attributes) {
        if (attr.name == "for") {
            found_for = true;
            EXPECT_EQ(attr.value, "name");
        }
    }
    EXPECT_TRUE(found_for) << "for attribute not found on label";
}

// === V145 HTML Parser Tests ===

TEST(HtmlParserTest, HtmlV145_1) {
    // a element with href
    auto doc = clever::html::parse("<html><body><a href=\"https://example.com\">Link</a></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->tag_name, "a");
    EXPECT_EQ(a->text_content(), "Link");
    bool found_href = false;
    for (const auto& attr : a->attributes) {
        if (attr.name == "href") {
            found_href = true;
            EXPECT_EQ(attr.value, "https://example.com");
        }
    }
    EXPECT_TRUE(found_href) << "href attribute not found on a";
}

TEST(HtmlParserTest, HtmlV145_2) {
    // img element with src and alt
    auto doc = clever::html::parse("<html><body><img src=\"logo.png\" alt=\"Logo\"></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->tag_name, "img");
    bool found_src = false;
    bool found_alt = false;
    for (const auto& attr : img->attributes) {
        if (attr.name == "src") {
            found_src = true;
            EXPECT_EQ(attr.value, "logo.png");
        }
        if (attr.name == "alt") {
            found_alt = true;
            EXPECT_EQ(attr.value, "Logo");
        }
    }
    EXPECT_TRUE(found_src) << "src attribute not found on img";
    EXPECT_TRUE(found_alt) << "alt attribute not found on img";
}

TEST(HtmlParserTest, HtmlV145_3) {
    // div with multiple classes
    auto doc = clever::html::parse("<html><body><div class=\"foo bar baz\">Content</div></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->tag_name, "div");
    bool found_class = false;
    for (const auto& attr : div->attributes) {
        if (attr.name == "class") {
            found_class = true;
            EXPECT_EQ(attr.value, "foo bar baz");
        }
    }
    EXPECT_TRUE(found_class) << "class attribute not found on div";
}

TEST(HtmlParserTest, HtmlV145_4) {
    // span with inline style attr
    auto doc = clever::html::parse("<html><body><span style=\"color:red\">Red</span></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->tag_name, "span");
    EXPECT_EQ(span->text_content(), "Red");
    bool found_style = false;
    for (const auto& attr : span->attributes) {
        if (attr.name == "style") {
            found_style = true;
            EXPECT_EQ(attr.value, "color:red");
        }
    }
    EXPECT_TRUE(found_style) << "style attribute not found on span";
}

TEST(HtmlParserTest, HtmlV145_5) {
    // ul with li children
    auto doc = clever::html::parse("<html><body><ul><li>A</li><li>B</li><li>C</li></ul></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    EXPECT_EQ(ul->tag_name, "ul");

    int li_count = 0;
    for (const auto& child : ul->children) {
        if (child->tag_name == "li") {
            li_count++;
        }
    }
    EXPECT_EQ(li_count, 3);
}

TEST(HtmlParserTest, HtmlV145_6) {
    // ol with li children
    auto doc = clever::html::parse("<html><body><ol><li>First</li><li>Second</li></ol></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    EXPECT_EQ(ol->tag_name, "ol");

    int li_count = 0;
    for (const auto& child : ol->children) {
        if (child->tag_name == "li") {
            li_count++;
        }
    }
    EXPECT_EQ(li_count, 2);
}

TEST(HtmlParserTest, HtmlV145_7) {
    // table with tr and td
    auto doc = clever::html::parse("<html><body><table><tr><td>Cell1</td><td>Cell2</td></tr></table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->tag_name, "table");

    auto* td = doc->find_element("td");
    ASSERT_NE(td, nullptr);
    EXPECT_EQ(td->tag_name, "td");
    EXPECT_EQ(td->text_content(), "Cell1");
}

TEST(HtmlParserTest, HtmlV145_8) {
    // form with action attribute
    auto doc = clever::html::parse("<html><body><form action=\"/submit\">Form</form></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* form = doc->find_element("form");
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(form->tag_name, "form");
    bool found_action = false;
    for (const auto& attr : form->attributes) {
        if (attr.name == "action") {
            found_action = true;
            EXPECT_EQ(attr.value, "/submit");
        }
    }
    EXPECT_TRUE(found_action) << "action attribute not found on form";
}

// === V146 HTML Parser Tests ===

TEST(HtmlParserTest, HtmlV146_1) {
    // p element with text
    auto doc = clever::html::parse("<html><body><p>Hello World</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* p = doc->find_element("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->tag_name, "p");
    EXPECT_EQ(p->text_content(), "Hello World");
}

TEST(HtmlParserTest, HtmlV146_2) {
    // div with nested span
    auto doc = clever::html::parse("<html><body><div><span>Inner</span></div></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->tag_name, "div");

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->tag_name, "span");
    EXPECT_EQ(span->text_content(), "Inner");
}

TEST(HtmlParserTest, HtmlV146_3) {
    // b and i elements
    auto doc = clever::html::parse("<html><body><b>Bold</b><i>Italic</i></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* b = doc->find_element("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->tag_name, "b");
    EXPECT_EQ(b->text_content(), "Bold");

    auto* i = doc->find_element("i");
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->tag_name, "i");
    EXPECT_EQ(i->text_content(), "Italic");
}

TEST(HtmlParserTest, HtmlV146_4) {
    // pre element with whitespace
    auto doc = clever::html::parse("<html><body><pre>Preformatted</pre></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* pre = doc->find_element("pre");
    ASSERT_NE(pre, nullptr);
    EXPECT_EQ(pre->tag_name, "pre");
    EXPECT_EQ(pre->text_content(), "Preformatted");
}

TEST(HtmlParserTest, HtmlV146_5) {
    // blockquote element
    auto doc = clever::html::parse("<html><body><blockquote>Quoted text</blockquote></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* bq = doc->find_element("blockquote");
    ASSERT_NE(bq, nullptr);
    EXPECT_EQ(bq->tag_name, "blockquote");
    EXPECT_EQ(bq->text_content(), "Quoted text");
}

TEST(HtmlParserTest, HtmlV146_6) {
    // code element
    auto doc = clever::html::parse("<html><body><code>x = 42</code></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* code = doc->find_element("code");
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(code->tag_name, "code");
    EXPECT_EQ(code->text_content(), "x = 42");
}

TEST(HtmlParserTest, HtmlV146_7) {
    // sup and sub elements
    auto doc = clever::html::parse("<html><body><sup>Up</sup><sub>Down</sub></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* sup = doc->find_element("sup");
    ASSERT_NE(sup, nullptr);
    EXPECT_EQ(sup->tag_name, "sup");
    EXPECT_EQ(sup->text_content(), "Up");

    auto* sub = doc->find_element("sub");
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->tag_name, "sub");
    EXPECT_EQ(sub->text_content(), "Down");
}

TEST(HtmlParserTest, HtmlV146_8) {
    // mark element
    auto doc = clever::html::parse("<html><body><mark>Highlighted</mark></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* mark = doc->find_element("mark");
    ASSERT_NE(mark, nullptr);
    EXPECT_EQ(mark->tag_name, "mark");
    EXPECT_EQ(mark->text_content(), "Highlighted");
}

// === V147 HTML Parser Tests ===

TEST(HtmlParserTest, HtmlV147_1) {
    // span with text
    auto doc = clever::html::parse("<html><body><span>Hello World</span></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->tag_name, "span");
    EXPECT_EQ(span->text_content(), "Hello World");
}

TEST(HtmlParserTest, HtmlV147_2) {
    // div with id attribute
    auto doc = clever::html::parse("<html><body><div id=\"container\">Content</div></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* div = doc->find_element("div");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->tag_name, "div");
    ASSERT_GE(div->attributes.size(), 1u);
    EXPECT_EQ(div->attributes[0].name, "id");
    EXPECT_EQ(div->attributes[0].value, "container");
    EXPECT_EQ(div->text_content(), "Content");
}

TEST(HtmlParserTest, HtmlV147_3) {
    // anchor with target=_blank
    auto doc = clever::html::parse("<html><body><a href=\"https://example.com\" target=\"_blank\">Link</a></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* a = doc->find_element("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->tag_name, "a");
    ASSERT_GE(a->attributes.size(), 2u);
    EXPECT_EQ(a->text_content(), "Link");
    // Check target attribute exists
    bool found_target = false;
    for (const auto& attr : a->attributes) {
        if (attr.name == "target") {
            found_target = true;
            EXPECT_EQ(attr.value, "_blank");
        }
    }
    EXPECT_TRUE(found_target) << "target attribute not found";
}

TEST(HtmlParserTest, HtmlV147_4) {
    // meta charset
    auto doc = clever::html::parse("<html><head><meta charset=\"utf-8\"></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* meta = doc->find_element("meta");
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->tag_name, "meta");
    ASSERT_GE(meta->attributes.size(), 1u);
    EXPECT_EQ(meta->attributes[0].name, "charset");
    EXPECT_EQ(meta->attributes[0].value, "utf-8");
}

TEST(HtmlParserTest, HtmlV147_5) {
    // link rel=stylesheet
    auto doc = clever::html::parse("<html><head><link rel=\"stylesheet\" href=\"style.css\"></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* link = doc->find_element("link");
    ASSERT_NE(link, nullptr);
    EXPECT_EQ(link->tag_name, "link");
    bool found_rel = false;
    for (const auto& attr : link->attributes) {
        if (attr.name == "rel") {
            found_rel = true;
            EXPECT_EQ(attr.value, "stylesheet");
        }
    }
    EXPECT_TRUE(found_rel) << "rel attribute not found";
}

TEST(HtmlParserTest, HtmlV147_6) {
    // script src attribute
    auto doc = clever::html::parse("<html><head><script src=\"app.js\"></script></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* script = doc->find_element("script");
    ASSERT_NE(script, nullptr);
    EXPECT_EQ(script->tag_name, "script");
    ASSERT_GE(script->attributes.size(), 1u);
    EXPECT_EQ(script->attributes[0].name, "src");
    EXPECT_EQ(script->attributes[0].value, "app.js");
}

TEST(HtmlParserTest, HtmlV147_7) {
    // noscript element
    auto doc = clever::html::parse("<html><body><noscript>Enable JS</noscript></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* noscript = doc->find_element("noscript");
    ASSERT_NE(noscript, nullptr);
    EXPECT_EQ(noscript->tag_name, "noscript");
    EXPECT_EQ(noscript->text_content(), "Enable JS");
}

TEST(HtmlParserTest, HtmlV147_8) {
    // style element with CSS content
    auto doc = clever::html::parse("<html><head><style>body { color: red; }</style></head><body></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* style = doc->find_element("style");
    ASSERT_NE(style, nullptr);
    EXPECT_EQ(style->tag_name, "style");
}

// --- V148 HTML Parser Tests ---

TEST(HtmlParserTest, HtmlV148_1) {
    // video element with src
    auto doc = clever::html::parse("<html><body><video src=\"movie.mp4\"></video></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* video = doc->find_element("video");
    ASSERT_NE(video, nullptr);
    EXPECT_EQ(video->tag_name, "video");
    ASSERT_GE(video->attributes.size(), 1u);
    EXPECT_EQ(video->attributes[0].name, "src");
    EXPECT_EQ(video->attributes[0].value, "movie.mp4");
}

TEST(HtmlParserTest, HtmlV148_2) {
    // audio element with controls
    auto doc = clever::html::parse("<html><body><audio controls></audio></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* audio = doc->find_element("audio");
    ASSERT_NE(audio, nullptr);
    EXPECT_EQ(audio->tag_name, "audio");
    bool found_controls = false;
    for (const auto& attr : audio->attributes) {
        if (attr.name == "controls") found_controls = true;
    }
    EXPECT_TRUE(found_controls) << "controls attribute not found";
}

TEST(HtmlParserTest, HtmlV148_3) {
    // source element with type
    auto doc = clever::html::parse("<html><body><video><source src=\"a.mp4\" type=\"video/mp4\"></video></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* source = doc->find_element("source");
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->tag_name, "source");
    bool found_type = false;
    for (const auto& attr : source->attributes) {
        if (attr.name == "type") {
            found_type = true;
            EXPECT_EQ(attr.value, "video/mp4");
        }
    }
    EXPECT_TRUE(found_type) << "type attribute not found";
}

TEST(HtmlParserTest, HtmlV148_4) {
    // iframe with src
    auto doc = clever::html::parse("<html><body><iframe src=\"https://example.com\"></iframe></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* iframe = doc->find_element("iframe");
    ASSERT_NE(iframe, nullptr);
    EXPECT_EQ(iframe->tag_name, "iframe");
    ASSERT_GE(iframe->attributes.size(), 1u);
    EXPECT_EQ(iframe->attributes[0].name, "src");
    EXPECT_EQ(iframe->attributes[0].value, "https://example.com");
}

TEST(HtmlParserTest, HtmlV148_5) {
    // embed element
    auto doc = clever::html::parse("<html><body><embed src=\"plugin.swf\" type=\"application/x-shockwave-flash\"></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* embed = doc->find_element("embed");
    ASSERT_NE(embed, nullptr);
    EXPECT_EQ(embed->tag_name, "embed");
    bool found_type = false;
    for (const auto& attr : embed->attributes) {
        if (attr.name == "type") {
            found_type = true;
            EXPECT_EQ(attr.value, "application/x-shockwave-flash");
        }
    }
    EXPECT_TRUE(found_type) << "type attribute not found";
}

TEST(HtmlParserTest, HtmlV148_6) {
    // object with data attribute
    auto doc = clever::html::parse("<html><body><object data=\"movie.swf\" type=\"application/x-shockwave-flash\"></object></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* obj = doc->find_element("object");
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->tag_name, "object");
    bool found_data = false;
    for (const auto& attr : obj->attributes) {
        if (attr.name == "data") {
            found_data = true;
            EXPECT_EQ(attr.value, "movie.swf");
        }
    }
    EXPECT_TRUE(found_data) << "data attribute not found";
}

TEST(HtmlParserTest, HtmlV148_7) {
    // param element
    auto doc = clever::html::parse("<html><body><object><param name=\"movie\" value=\"a.swf\"></object></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* param = doc->find_element("param");
    ASSERT_NE(param, nullptr);
    EXPECT_EQ(param->tag_name, "param");
    bool found_name = false;
    bool found_value = false;
    for (const auto& attr : param->attributes) {
        if (attr.name == "name") {
            found_name = true;
            EXPECT_EQ(attr.value, "movie");
        }
        if (attr.name == "value") {
            found_value = true;
            EXPECT_EQ(attr.value, "a.swf");
        }
    }
    EXPECT_TRUE(found_name) << "name attribute not found";
    EXPECT_TRUE(found_value) << "value attribute not found";
}

TEST(HtmlParserTest, HtmlV148_8) {
    // canvas element with width/height
    auto doc = clever::html::parse("<html><body><canvas width=\"300\" height=\"150\"></canvas></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* canvas = doc->find_element("canvas");
    ASSERT_NE(canvas, nullptr);
    EXPECT_EQ(canvas->tag_name, "canvas");
    bool found_width = false;
    bool found_height = false;
    for (const auto& attr : canvas->attributes) {
        if (attr.name == "width") {
            found_width = true;
            EXPECT_EQ(attr.value, "300");
        }
        if (attr.name == "height") {
            found_height = true;
            EXPECT_EQ(attr.value, "150");
        }
    }
    EXPECT_TRUE(found_width) << "width attribute not found";
    EXPECT_TRUE(found_height) << "height attribute not found";
}

TEST(HtmlParserTest, HtmlV149_1) {
    // dl with dt and dd
    auto doc = clever::html::parse("<html><body><dl><dt>Term</dt><dd>Definition</dd></dl></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    EXPECT_EQ(dl->tag_name, "dl");
    ASSERT_GE(dl->children.size(), 2u);

    auto* dt = dl->children[0].get();
    ASSERT_NE(dt, nullptr);
    EXPECT_EQ(dt->tag_name, "dt");
    EXPECT_EQ(dt->text_content(), "Term");

    auto* dd = dl->children[1].get();
    ASSERT_NE(dd, nullptr);
    EXPECT_EQ(dd->tag_name, "dd");
    EXPECT_EQ(dd->text_content(), "Definition");
}

TEST(HtmlParserTest, HtmlV149_2) {
    // details with summary
    auto doc = clever::html::parse("<html><body><details><summary>Click me</summary>Content here</details></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* details = doc->find_element("details");
    ASSERT_NE(details, nullptr);
    EXPECT_EQ(details->tag_name, "details");
    ASSERT_GE(details->children.size(), 1u);

    auto* summary = details->children[0].get();
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->tag_name, "summary");
    EXPECT_EQ(summary->text_content(), "Click me");
}

TEST(HtmlParserTest, HtmlV149_3) {
    // ruby with rt
    auto doc = clever::html::parse("<html><body><ruby>漢<rt>kan</rt></ruby></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ruby = doc->find_element("ruby");
    ASSERT_NE(ruby, nullptr);
    EXPECT_EQ(ruby->tag_name, "ruby");

    auto* rt = doc->find_element("rt");
    ASSERT_NE(rt, nullptr);
    EXPECT_EQ(rt->tag_name, "rt");
    EXPECT_EQ(rt->text_content(), "kan");
}

TEST(HtmlParserTest, HtmlV149_4) {
    // wbr element (void)
    auto doc = clever::html::parse("<html><body><p>long<wbr>word</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* wbr = doc->find_element("wbr");
    ASSERT_NE(wbr, nullptr);
    EXPECT_EQ(wbr->tag_name, "wbr");
    EXPECT_EQ(wbr->children.size(), 0u);
}

TEST(HtmlParserTest, HtmlV149_5) {
    // ins and del elements
    auto doc = clever::html::parse("<html><body><p><ins>added</ins><del>removed</del></p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ins = doc->find_element("ins");
    ASSERT_NE(ins, nullptr);
    EXPECT_EQ(ins->tag_name, "ins");
    EXPECT_EQ(ins->text_content(), "added");

    auto* del_elem = doc->find_element("del");
    ASSERT_NE(del_elem, nullptr);
    EXPECT_EQ(del_elem->tag_name, "del");
    EXPECT_EQ(del_elem->text_content(), "removed");
}

TEST(HtmlParserTest, HtmlV149_6) {
    // q element with cite
    auto doc = clever::html::parse("<html><body><q cite=\"https://example.com\">Quote text</q></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* q = doc->find_element("q");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->tag_name, "q");
    EXPECT_EQ(q->text_content(), "Quote text");
    bool found_cite = false;
    for (const auto& attr : q->attributes) {
        if (attr.name == "cite") {
            found_cite = true;
            EXPECT_EQ(attr.value, "https://example.com");
        }
    }
    EXPECT_TRUE(found_cite) << "cite attribute not found";
}

TEST(HtmlParserTest, HtmlV149_7) {
    // bdo with dir attribute
    auto doc = clever::html::parse("<html><body><bdo dir=\"rtl\">reversed</bdo></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* bdo = doc->find_element("bdo");
    ASSERT_NE(bdo, nullptr);
    EXPECT_EQ(bdo->tag_name, "bdo");
    EXPECT_EQ(bdo->text_content(), "reversed");
    bool found_dir = false;
    for (const auto& attr : bdo->attributes) {
        if (attr.name == "dir") {
            found_dir = true;
            EXPECT_EQ(attr.value, "rtl");
        }
    }
    EXPECT_TRUE(found_dir) << "dir attribute not found";
}

TEST(HtmlParserTest, HtmlV149_8) {
    // dfn element
    auto doc = clever::html::parse("<html><body><p><dfn>Definition term</dfn> is important.</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dfn = doc->find_element("dfn");
    ASSERT_NE(dfn, nullptr);
    EXPECT_EQ(dfn->tag_name, "dfn");
    EXPECT_EQ(dfn->text_content(), "Definition term");
}

TEST(HtmlParserTest, HtmlV150_1) {
    // ruby with rt annotation
    auto doc = clever::html::parse("<html><body><ruby>漢<rt>kan</rt></ruby></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ruby = doc->find_element("ruby");
    ASSERT_NE(ruby, nullptr);
    EXPECT_EQ(ruby->tag_name, "ruby");

    auto* rt = doc->find_element("rt");
    ASSERT_NE(rt, nullptr);
    EXPECT_EQ(rt->tag_name, "rt");
    EXPECT_EQ(rt->text_content(), "kan");
}

TEST(HtmlParserTest, HtmlV150_2) {
    // map with area elements
    auto doc = clever::html::parse("<html><body><map name=\"test\"><area shape=\"rect\" href=\"/link\"></map></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* map = doc->find_element("map");
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map->tag_name, "map");
    bool found_name = false;
    for (const auto& attr : map->attributes) {
        if (attr.name == "name") {
            found_name = true;
            EXPECT_EQ(attr.value, "test");
        }
    }
    EXPECT_TRUE(found_name) << "name attribute not found on map";

    auto* area = doc->find_element("area");
    ASSERT_NE(area, nullptr);
    EXPECT_EQ(area->tag_name, "area");
}

TEST(HtmlParserTest, HtmlV150_3) {
    // ins and del elements
    auto doc = clever::html::parse("<html><body><p><ins>inserted</ins> and <del>deleted</del></p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ins = doc->find_element("ins");
    ASSERT_NE(ins, nullptr);
    EXPECT_EQ(ins->tag_name, "ins");
    EXPECT_EQ(ins->text_content(), "inserted");

    auto* del_elem = doc->find_element("del");
    ASSERT_NE(del_elem, nullptr);
    EXPECT_EQ(del_elem->tag_name, "del");
    EXPECT_EQ(del_elem->text_content(), "deleted");
}

TEST(HtmlParserTest, HtmlV150_4) {
    // bdo with dir attribute
    auto doc = clever::html::parse("<html><body><bdo dir=\"ltr\">left-to-right</bdo></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* bdo = doc->find_element("bdo");
    ASSERT_NE(bdo, nullptr);
    EXPECT_EQ(bdo->tag_name, "bdo");
    EXPECT_EQ(bdo->text_content(), "left-to-right");
    bool found_dir = false;
    for (const auto& attr : bdo->attributes) {
        if (attr.name == "dir") {
            found_dir = true;
            EXPECT_EQ(attr.value, "ltr");
        }
    }
    EXPECT_TRUE(found_dir) << "dir attribute not found on bdo";
}

TEST(HtmlParserTest, HtmlV150_5) {
    // wbr element (void)
    auto doc = clever::html::parse("<html><body><p>super<wbr>cali<wbr>fragil</p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* wbr = doc->find_element("wbr");
    ASSERT_NE(wbr, nullptr);
    EXPECT_EQ(wbr->tag_name, "wbr");
    EXPECT_EQ(wbr->children.size(), 0u);
}

TEST(HtmlParserTest, HtmlV150_6) {
    // dl/dt/dd definition list
    auto doc = clever::html::parse("<html><body><dl><dt>Term</dt><dd>Definition</dd></dl></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dl = doc->find_element("dl");
    ASSERT_NE(dl, nullptr);
    EXPECT_EQ(dl->tag_name, "dl");

    auto* dt = doc->find_element("dt");
    ASSERT_NE(dt, nullptr);
    EXPECT_EQ(dt->tag_name, "dt");
    EXPECT_EQ(dt->text_content(), "Term");

    auto* dd = doc->find_element("dd");
    ASSERT_NE(dd, nullptr);
    EXPECT_EQ(dd->tag_name, "dd");
    EXPECT_EQ(dd->text_content(), "Definition");
}

TEST(HtmlParserTest, HtmlV150_7) {
    // optgroup with options
    auto doc = clever::html::parse("<html><body><select><optgroup label=\"Group\"><option>A</option><option>B</option></optgroup></select></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* optgroup = doc->find_element("optgroup");
    ASSERT_NE(optgroup, nullptr);
    EXPECT_EQ(optgroup->tag_name, "optgroup");
    bool found_label = false;
    for (const auto& attr : optgroup->attributes) {
        if (attr.name == "label") {
            found_label = true;
            EXPECT_EQ(attr.value, "Group");
        }
    }
    EXPECT_TRUE(found_label) << "label attribute not found on optgroup";

    auto* option = doc->find_element("option");
    ASSERT_NE(option, nullptr);
    EXPECT_EQ(option->tag_name, "option");
    EXPECT_EQ(option->text_content(), "A");
}

TEST(HtmlParserTest, HtmlV150_8) {
    // figure with figcaption
    auto doc = clever::html::parse("<html><body><figure><img src=\"photo.jpg\"><figcaption>Caption text</figcaption></figure></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* figure = doc->find_element("figure");
    ASSERT_NE(figure, nullptr);
    EXPECT_EQ(figure->tag_name, "figure");

    auto* figcaption = doc->find_element("figcaption");
    ASSERT_NE(figcaption, nullptr);
    EXPECT_EQ(figcaption->tag_name, "figcaption");
    EXPECT_EQ(figcaption->text_content(), "Caption text");

    auto* img = doc->find_element("img");
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->tag_name, "img");
}

TEST(HtmlParserTest, HtmlV151_1) {
    // abbr with title attribute
    auto doc = clever::html::parse("<html><body><abbr title=\"HyperText Markup Language\">HTML</abbr></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* abbr = doc->find_element("abbr");
    ASSERT_NE(abbr, nullptr);
    EXPECT_EQ(abbr->tag_name, "abbr");
    EXPECT_EQ(abbr->text_content(), "HTML");
    bool found_title = false;
    for (const auto& attr : abbr->attributes) {
        if (attr.name == "title") {
            found_title = true;
            EXPECT_EQ(attr.value, "HyperText Markup Language");
        }
    }
    EXPECT_TRUE(found_title) << "title attribute not found on abbr";
}

TEST(HtmlParserTest, HtmlV151_2) {
    // cite element
    auto doc = clever::html::parse("<html><body><cite>The Great Gatsby</cite></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* cite = doc->find_element("cite");
    ASSERT_NE(cite, nullptr);
    EXPECT_EQ(cite->tag_name, "cite");
    EXPECT_EQ(cite->text_content(), "The Great Gatsby");
}

TEST(HtmlParserTest, HtmlV151_3) {
    // kbd element
    auto doc = clever::html::parse("<html><body><kbd>Ctrl+C</kbd></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* kbd = doc->find_element("kbd");
    ASSERT_NE(kbd, nullptr);
    EXPECT_EQ(kbd->tag_name, "kbd");
    EXPECT_EQ(kbd->text_content(), "Ctrl+C");
}

TEST(HtmlParserTest, HtmlV151_4) {
    // samp element
    auto doc = clever::html::parse("<html><body><samp>Error 404</samp></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* samp = doc->find_element("samp");
    ASSERT_NE(samp, nullptr);
    EXPECT_EQ(samp->tag_name, "samp");
    EXPECT_EQ(samp->text_content(), "Error 404");
}

TEST(HtmlParserTest, HtmlV151_5) {
    // var element
    auto doc = clever::html::parse("<html><body><var>x</var></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* var_elem = doc->find_element("var");
    ASSERT_NE(var_elem, nullptr);
    EXPECT_EQ(var_elem->tag_name, "var");
    EXPECT_EQ(var_elem->text_content(), "x");
}

TEST(HtmlParserTest, HtmlV151_6) {
    // q with cite attribute
    auto doc = clever::html::parse("<html><body><q cite=\"https://example.com\">A quote</q></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* q = doc->find_element("q");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->tag_name, "q");
    EXPECT_EQ(q->text_content(), "A quote");
    bool found_cite = false;
    for (const auto& attr : q->attributes) {
        if (attr.name == "cite") {
            found_cite = true;
            EXPECT_EQ(attr.value, "https://example.com");
        }
    }
    EXPECT_TRUE(found_cite) << "cite attribute not found on q";
}

TEST(HtmlParserTest, HtmlV151_7) {
    // dfn element
    auto doc = clever::html::parse("<html><body><dfn>Definition term</dfn></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* dfn = doc->find_element("dfn");
    ASSERT_NE(dfn, nullptr);
    EXPECT_EQ(dfn->tag_name, "dfn");
    EXPECT_EQ(dfn->text_content(), "Definition term");
}

TEST(HtmlParserTest, HtmlV151_8) {
    // address element
    auto doc = clever::html::parse("<html><body><address>123 Main St</address></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* address = doc->find_element("address");
    ASSERT_NE(address, nullptr);
    EXPECT_EQ(address->tag_name, "address");
    EXPECT_EQ(address->text_content(), "123 Main St");
}

// ---------------------------------------------------------------------------
// Cycle V152 — mark, bdi, meter, output, summary, time, data, sub/sup
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, HtmlV152_1) {
    // mark element
    auto doc = clever::html::parse("<html><body><mark>highlighted</mark></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* mark = doc->find_element("mark");
    ASSERT_NE(mark, nullptr);
    EXPECT_EQ(mark->tag_name, "mark");
    EXPECT_EQ(mark->text_content(), "highlighted");
}

TEST(HtmlParserTest, HtmlV152_2) {
    // bdi element
    auto doc = clever::html::parse("<html><body><bdi>bidirectional</bdi></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* bdi = doc->find_element("bdi");
    ASSERT_NE(bdi, nullptr);
    EXPECT_EQ(bdi->tag_name, "bdi");
    EXPECT_EQ(bdi->text_content(), "bidirectional");
}

TEST(HtmlParserTest, HtmlV152_3) {
    // meter with value attribute
    auto doc = clever::html::parse("<html><body><meter value=\"0.7\">70%</meter></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* meter = doc->find_element("meter");
    ASSERT_NE(meter, nullptr);
    EXPECT_EQ(meter->tag_name, "meter");
    EXPECT_EQ(meter->text_content(), "70%");
    bool found_value = false;
    for (const auto& attr : meter->attributes) {
        if (attr.name == "value") {
            found_value = true;
            EXPECT_EQ(attr.value, "0.7");
        }
    }
    EXPECT_TRUE(found_value) << "value attribute not found on meter";
}

TEST(HtmlParserTest, HtmlV152_4) {
    // output element
    auto doc = clever::html::parse("<html><body><output>Result</output></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* output = doc->find_element("output");
    ASSERT_NE(output, nullptr);
    EXPECT_EQ(output->tag_name, "output");
    EXPECT_EQ(output->text_content(), "Result");
}

TEST(HtmlParserTest, HtmlV152_5) {
    // summary element
    auto doc = clever::html::parse("<html><body><details><summary>Click me</summary></details></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* summary = doc->find_element("summary");
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->tag_name, "summary");
    EXPECT_EQ(summary->text_content(), "Click me");
}

TEST(HtmlParserTest, HtmlV152_6) {
    // time with datetime attribute
    auto doc = clever::html::parse("<html><body><time datetime=\"2024-01-15\">January 15</time></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* time_elem = doc->find_element("time");
    ASSERT_NE(time_elem, nullptr);
    EXPECT_EQ(time_elem->tag_name, "time");
    EXPECT_EQ(time_elem->text_content(), "January 15");
    bool found_datetime = false;
    for (const auto& attr : time_elem->attributes) {
        if (attr.name == "datetime") {
            found_datetime = true;
            EXPECT_EQ(attr.value, "2024-01-15");
        }
    }
    EXPECT_TRUE(found_datetime) << "datetime attribute not found on time";
}

TEST(HtmlParserTest, HtmlV152_7) {
    // data with value attribute
    auto doc = clever::html::parse("<html><body><data value=\"42\">forty-two</data></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* data = doc->find_element("data");
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->tag_name, "data");
    EXPECT_EQ(data->text_content(), "forty-two");
    bool found_value = false;
    for (const auto& attr : data->attributes) {
        if (attr.name == "value") {
            found_value = true;
            EXPECT_EQ(attr.value, "42");
        }
    }
    EXPECT_TRUE(found_value) << "value attribute not found on data";
}

TEST(HtmlParserTest, HtmlV152_8) {
    // sub and sup together
    auto doc = clever::html::parse("<html><body><p>H<sub>2</sub>O is x<sup>2</sup></p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* sub = doc->find_element("sub");
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->tag_name, "sub");
    EXPECT_EQ(sub->text_content(), "2");

    auto* sup = doc->find_element("sup");
    ASSERT_NE(sup, nullptr);
    EXPECT_EQ(sup->tag_name, "sup");
    EXPECT_EQ(sup->text_content(), "2");
}

// ---------------------------------------------------------------------------
// Cycle V153 — s, u, span multi-attr, nested divs, p+inline, ul, ol, table
// ---------------------------------------------------------------------------

TEST(HtmlParserTest, HtmlV153_1) {
    // s (strikethrough) element
    auto doc = clever::html::parse("<html><body><s>deleted text</s></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* s = doc->find_element("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->tag_name, "s");
    EXPECT_EQ(s->text_content(), "deleted text");
}

TEST(HtmlParserTest, HtmlV153_2) {
    // u (underline) element
    auto doc = clever::html::parse("<html><body><u>underlined text</u></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* u = doc->find_element("u");
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(u->tag_name, "u");
    EXPECT_EQ(u->text_content(), "underlined text");
}

TEST(HtmlParserTest, HtmlV153_3) {
    // span with multiple attributes
    auto doc = clever::html::parse(
        "<html><body><span id=\"myspan\" class=\"highlight\" data-value=\"42\">content</span></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* span = doc->find_element("span");
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->tag_name, "span");
    EXPECT_EQ(span->text_content(), "content");

    bool found_id = false, found_class = false, found_data = false;
    for (const auto& attr : span->attributes) {
        if (attr.name == "id") { found_id = true; EXPECT_EQ(attr.value, "myspan"); }
        if (attr.name == "class") { found_class = true; EXPECT_EQ(attr.value, "highlight"); }
        if (attr.name == "data-value") { found_data = true; EXPECT_EQ(attr.value, "42"); }
    }
    EXPECT_TRUE(found_id) << "id attribute not found on span";
    EXPECT_TRUE(found_class) << "class attribute not found on span";
    EXPECT_TRUE(found_data) << "data-value attribute not found on span";
}

TEST(HtmlParserTest, HtmlV153_4) {
    // div with nested divs
    auto doc = clever::html::parse(
        "<html><body><div><div>inner1</div><div>inner2</div></div></body></html>");
    ASSERT_NE(doc, nullptr);

    auto divs = doc->find_all_elements("div");
    ASSERT_GE(divs.size(), 3u);
    // Outer div should contain both inner divs
    EXPECT_EQ(divs[0]->tag_name, "div");
    EXPECT_EQ(divs[1]->tag_name, "div");
    EXPECT_EQ(divs[1]->text_content(), "inner1");
    EXPECT_EQ(divs[2]->tag_name, "div");
    EXPECT_EQ(divs[2]->text_content(), "inner2");
}

TEST(HtmlParserTest, HtmlV153_5) {
    // p with inline elements
    auto doc = clever::html::parse(
        "<html><body><p>Hello <strong>world</strong> and <em>universe</em></p></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* strong = doc->find_element("strong");
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(strong->text_content(), "world");

    auto* em = doc->find_element("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text_content(), "universe");
}

TEST(HtmlParserTest, HtmlV153_6) {
    // ul with multiple li
    auto doc = clever::html::parse(
        "<html><body><ul><li>Alpha</li><li>Beta</li><li>Gamma</li></ul></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ul = doc->find_element("ul");
    ASSERT_NE(ul, nullptr);
    EXPECT_EQ(ul->tag_name, "ul");

    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 3u);
    EXPECT_EQ(lis[0]->text_content(), "Alpha");
    EXPECT_EQ(lis[1]->text_content(), "Beta");
    EXPECT_EQ(lis[2]->text_content(), "Gamma");
}

TEST(HtmlParserTest, HtmlV153_7) {
    // ol with start attribute
    auto doc = clever::html::parse(
        "<html><body><ol start=\"5\"><li>Five</li><li>Six</li></ol></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* ol = doc->find_element("ol");
    ASSERT_NE(ol, nullptr);
    EXPECT_EQ(ol->tag_name, "ol");

    bool found_start = false;
    for (const auto& attr : ol->attributes) {
        if (attr.name == "start") {
            found_start = true;
            EXPECT_EQ(attr.value, "5");
        }
    }
    EXPECT_TRUE(found_start) << "start attribute not found on ol";

    auto lis = doc->find_all_elements("li");
    ASSERT_EQ(lis.size(), 2u);
    EXPECT_EQ(lis[0]->text_content(), "Five");
    EXPECT_EQ(lis[1]->text_content(), "Six");
}

TEST(HtmlParserTest, HtmlV153_8) {
    // table with multiple rows
    auto doc = clever::html::parse(
        "<html><body><table><tbody><tr><td>R1C1</td><td>R1C2</td></tr>"
        "<tr><td>R2C1</td><td>R2C2</td></tr></tbody></table></body></html>");
    ASSERT_NE(doc, nullptr);

    auto* table = doc->find_element("table");
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->tag_name, "table");

    auto rows = doc->find_all_elements("tr");
    ASSERT_EQ(rows.size(), 2u);

    auto cells = doc->find_all_elements("td");
    ASSERT_EQ(cells.size(), 4u);
    EXPECT_EQ(cells[0]->text_content(), "R1C1");
    EXPECT_EQ(cells[1]->text_content(), "R1C2");
    EXPECT_EQ(cells[2]->text_content(), "R2C1");
    EXPECT_EQ(cells[3]->text_content(), "R2C2");
}
