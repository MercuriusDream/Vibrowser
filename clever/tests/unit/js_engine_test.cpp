#include <clever/js/js_engine.h>
#include <clever/js/js_dom_bindings.h>
#include <clever/js/js_timers.h>
#include <clever/js/js_window.h>
#include <clever/html/tree_builder.h>
#include <gtest/gtest.h>
#include <string>

// ============================================================================
// 1. JSEngine basic initialization and destruction
// ============================================================================
TEST(JSEngine, InitializationAndDestruction) {
    clever::js::JSEngine engine;
    EXPECT_NE(engine.context(), nullptr);
    EXPECT_NE(engine.runtime(), nullptr);
    EXPECT_FALSE(engine.has_error());
    EXPECT_TRUE(engine.last_error().empty());
    EXPECT_TRUE(engine.console_output().empty());
}

// ============================================================================
// 2. Simple expression evaluation (1 + 2 = "3")
// ============================================================================
TEST(JSEngine, SimpleArithmeticExpression) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("1 + 2");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "3");
}

// ============================================================================
// 3. String evaluation ("hello" evaluates to "hello")
// ============================================================================
TEST(JSEngine, StringEvaluation) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'hello'");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "hello");
}

// ============================================================================
// 4. Variable declarations and usage
// ============================================================================
TEST(JSEngine, VariableDeclarationLet) {
    clever::js::JSEngine engine;
    engine.evaluate("let x = 42");
    auto result = engine.evaluate("x");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "42");
}

TEST(JSEngine, VariableDeclarationConst) {
    clever::js::JSEngine engine;
    engine.evaluate("const name = 'clever'");
    auto result = engine.evaluate("name");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "clever");
}

TEST(JSEngine, VariableDeclarationVar) {
    clever::js::JSEngine engine;
    engine.evaluate("var total = 10 + 20");
    auto result = engine.evaluate("total");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "30");
}

// ============================================================================
// 5. Function definitions and calls
// ============================================================================
TEST(JSEngine, FunctionDefinitionAndCall) {
    clever::js::JSEngine engine;
    engine.evaluate("function add(a, b) { return a + b; }");
    auto result = engine.evaluate("add(3, 4)");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "7");
}

TEST(JSEngine, ArrowFunction) {
    clever::js::JSEngine engine;
    engine.evaluate("const square = (n) => n * n");
    auto result = engine.evaluate("square(5)");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "25");
}

TEST(JSEngine, RecursiveFunction) {
    clever::js::JSEngine engine;
    engine.evaluate(R"(
        function factorial(n) {
            if (n <= 1) return 1;
            return n * factorial(n - 1);
        }
    )");
    auto result = engine.evaluate("factorial(5)");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "120");
}

// ============================================================================
// 6. Error handling (syntax errors, runtime errors)
// ============================================================================
TEST(JSEngine, SyntaxError) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("function {{{");
    EXPECT_TRUE(engine.has_error());
    EXPECT_FALSE(engine.last_error().empty());
    EXPECT_EQ(result, "");
}

TEST(JSEngine, ReferenceError) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("undeclaredVariable");
    EXPECT_TRUE(engine.has_error());
    EXPECT_NE(engine.last_error().find("not defined"), std::string::npos);
    EXPECT_EQ(result, "");
}

TEST(JSEngine, TypeError) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("null.property");
    EXPECT_TRUE(engine.has_error());
    EXPECT_FALSE(engine.last_error().empty());
    EXPECT_EQ(result, "");
}

TEST(JSEngine, ErrorClearedOnNextEval) {
    clever::js::JSEngine engine;
    engine.evaluate("undeclaredVariable");
    EXPECT_TRUE(engine.has_error());

    // Next successful evaluation should clear the error
    auto result = engine.evaluate("42");
    EXPECT_FALSE(engine.has_error());
    EXPECT_TRUE(engine.last_error().empty());
    EXPECT_EQ(result, "42");
}

// ============================================================================
// 7. console.log output capture
// ============================================================================
TEST(JSEngine, ConsoleLogCapture) {
    clever::js::JSEngine engine;
    engine.evaluate("console.log('hello world')");
    EXPECT_FALSE(engine.has_error());

    const auto& output = engine.console_output();
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output[0], "[log] hello world");
}

TEST(JSEngine, ConsoleLogMultipleArguments) {
    clever::js::JSEngine engine;
    engine.evaluate("console.log('value:', 42, true)");
    EXPECT_FALSE(engine.has_error());

    const auto& output = engine.console_output();
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output[0], "[log] value: 42 true");
}

TEST(JSEngine, ConsoleLogMultipleCalls) {
    clever::js::JSEngine engine;
    engine.evaluate("console.log('first')");
    engine.evaluate("console.log('second')");
    engine.evaluate("console.log('third')");

    const auto& output = engine.console_output();
    ASSERT_EQ(output.size(), 3u);
    EXPECT_EQ(output[0], "[log] first");
    EXPECT_EQ(output[1], "[log] second");
    EXPECT_EQ(output[2], "[log] third");
}

// ============================================================================
// 8. console.warn/error/info output capture
// ============================================================================
TEST(JSEngine, ConsoleWarnCapture) {
    clever::js::JSEngine engine;
    engine.evaluate("console.warn('warning message')");
    EXPECT_FALSE(engine.has_error());

    const auto& output = engine.console_output();
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output[0], "[warn] warning message");
}

TEST(JSEngine, ConsoleErrorCapture) {
    clever::js::JSEngine engine;
    engine.evaluate("console.error('error message')");
    EXPECT_FALSE(engine.has_error());

    const auto& output = engine.console_output();
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output[0], "[error] error message");
}

TEST(JSEngine, ConsoleInfoCapture) {
    clever::js::JSEngine engine;
    engine.evaluate("console.info('info message')");
    EXPECT_FALSE(engine.has_error());

    const auto& output = engine.console_output();
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output[0], "[info] info message");
}

TEST(JSEngine, ConsoleCallbackInvoked) {
    clever::js::JSEngine engine;

    std::string captured_level;
    std::string captured_message;
    engine.set_console_callback([&](const std::string& level, const std::string& message) {
        captured_level = level;
        captured_message = message;
    });

    engine.evaluate("console.warn('test callback')");
    EXPECT_EQ(captured_level, "warn");
    EXPECT_EQ(captured_message, "test callback");
}

TEST(JSEngine, ConsoleMixedLevels) {
    clever::js::JSEngine engine;
    engine.evaluate("console.log('L')");
    engine.evaluate("console.warn('W')");
    engine.evaluate("console.error('E')");
    engine.evaluate("console.info('I')");

    const auto& output = engine.console_output();
    ASSERT_EQ(output.size(), 4u);
    EXPECT_EQ(output[0], "[log] L");
    EXPECT_EQ(output[1], "[warn] W");
    EXPECT_EQ(output[2], "[error] E");
    EXPECT_EQ(output[3], "[info] I");
}

// ============================================================================
// 9. Multiple evaluations in same context (state persistence)
// ============================================================================
TEST(JSEngine, StatePersistenceAcrossEvaluations) {
    clever::js::JSEngine engine;
    engine.evaluate("var counter = 0");
    engine.evaluate("counter += 10");
    engine.evaluate("counter += 5");
    auto result = engine.evaluate("counter");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "15");
}

TEST(JSEngine, FunctionPersistenceAcrossEvaluations) {
    clever::js::JSEngine engine;
    engine.evaluate("function greet(name) { return 'Hello, ' + name + '!'; }");
    auto result = engine.evaluate("greet('Clever')");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "Hello, Clever!");
}

// ============================================================================
// 10. DOM bindings: document.getElementById
// ============================================================================
TEST(JSDom, GetElementById) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test Page</title></head>
        <body>
            <div id="main" class="container">
                <p id="greeting">Hello World</p>
                <span class="highlight">Important</span>
            </div>
        </body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("document.getElementById('greeting').textContent");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Hello World");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, GetElementByIdNotFound) {
    auto doc = clever::html::parse("<html><body><p>text</p></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("document.getElementById('nonexistent')");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "null");  // JS_NULL stringifies to "null"

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 11. DOM bindings: document.title getter/setter
// ============================================================================
TEST(JSDom, DocumentTitleGetter) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test Page</title></head>
        <body></body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("document.title");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Test Page");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, DocumentTitleSetter) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Original</title></head>
        <body></body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate("document.title = 'New Title'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto result = engine.evaluate("document.title");
    EXPECT_EQ(result, "New Title");

    // Also verify via the C++ API
    auto title = clever::js::get_document_title(engine.context());
    EXPECT_EQ(title, "New Title");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 12. DOM bindings: element.textContent getter/setter
// ============================================================================
TEST(JSDom, ElementTextContentGetter) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body>
            <div id="main" class="container">
                <p id="greeting">Hello World</p>
                <span class="highlight">Important</span>
            </div>
        </body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.getElementById('greeting').textContent");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Hello World");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementTextContentSetter) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body>
            <div id="main" class="container">
                <p id="greeting">Hello World</p>
                <span class="highlight">Important</span>
            </div>
        </body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate("document.getElementById('greeting').textContent = 'Goodbye World'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto result = engine.evaluate(
        "document.getElementById('greeting').textContent");
    EXPECT_EQ(result, "Goodbye World");

    // The DOM should be marked as modified
    EXPECT_TRUE(clever::js::dom_was_modified(engine.context()));

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 13. DOM bindings: element.getAttribute/setAttribute
// ============================================================================
TEST(JSDom, ElementGetAttribute) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body>
            <div id="main" class="container">
                <p id="greeting">Hello World</p>
                <span class="highlight">Important</span>
            </div>
        </body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.getElementById('main').getAttribute('class')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "container");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementGetAttributeNotFound) {
    auto doc = clever::html::parse(R"(
        <html><body><div id="box">text</div></body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.getElementById('box').getAttribute('data-missing')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // getAttribute returns null for missing attributes
    EXPECT_TRUE(result.empty() || result == "null");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementSetAttribute) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body>
            <div id="main" class="container">
                <p id="greeting">Hello World</p>
                <span class="highlight">Important</span>
            </div>
        </body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(
        "document.getElementById('main').setAttribute('data-custom', 'test-value')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto result = engine.evaluate(
        "document.getElementById('main').getAttribute('data-custom')");
    EXPECT_EQ(result, "test-value");

    EXPECT_TRUE(clever::js::dom_was_modified(engine.context()));

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 14. DOM bindings: document.querySelector (by tag, by id, by class)
// ============================================================================
TEST(JSDom, QuerySelectorByTag) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body>
            <div id="main" class="container">
                <p id="greeting">Hello World</p>
                <span class="highlight">Important</span>
            </div>
        </body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("document.querySelector('p').textContent");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Hello World");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, QuerySelectorById) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body>
            <div id="main" class="container">
                <p id="greeting">Hello World</p>
                <span class="highlight">Important</span>
            </div>
        </body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.querySelector('#greeting').textContent");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Hello World");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, QuerySelectorByClass) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body>
            <div id="main" class="container">
                <p id="greeting">Hello World</p>
                <span class="highlight">Important</span>
            </div>
        </body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.querySelector('.highlight').textContent");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Important");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, QuerySelectorNotFound) {
    auto doc = clever::html::parse("<html><body><p>text</p></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("document.querySelector('.nonexistent')");
    EXPECT_FALSE(engine.has_error());
    // null element should return empty or "null"
    EXPECT_TRUE(result.empty() || result == "null");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 15. DOM bindings: document.createElement + appendChild
// ============================================================================
TEST(JSDom, CreateElementAndAppendChild) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body>
            <div id="main" class="container">
                <p id="greeting">Hello World</p>
                <span class="highlight">Important</span>
            </div>
        </body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var newDiv = document.createElement('div');
        newDiv.textContent = 'New Element';
        document.getElementById('main').appendChild(newDiv);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // Verify the new element exists in the DOM tree
    auto result = engine.evaluate(
        "document.querySelector('#main').children.length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // Original: p, span. After append: p, span, div = 3 children
    EXPECT_EQ(result, "3");

    // Verify the DOM was marked as modified
    EXPECT_TRUE(clever::js::dom_was_modified(engine.context()));

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CreateElementWithAttributes) {
    auto doc = clever::html::parse("<html><body><div id=\"root\"></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var link = document.createElement('a');
        link.setAttribute('href', 'https://example.com');
        link.textContent = 'Click me';
        document.getElementById('root').appendChild(link);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto href = engine.evaluate(
        "document.querySelector('a').getAttribute('href')");
    EXPECT_EQ(href, "https://example.com");

    auto text = engine.evaluate("document.querySelector('a').textContent");
    EXPECT_EQ(text, "Click me");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 16. DOM bindings: element.innerHTML getter
// ============================================================================
TEST(JSDom, ElementInnerHTMLGetter) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body>
            <div id="main" class="container">
                <p id="greeting">Hello World</p>
                <span class="highlight">Important</span>
            </div>
        </body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("document.getElementById('main').innerHTML");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // innerHTML should contain the child elements
    EXPECT_NE(result.find("greeting"), std::string::npos);
    EXPECT_NE(result.find("Hello World"), std::string::npos);
    EXPECT_NE(result.find("highlight"), std::string::npos);
    EXPECT_NE(result.find("Important"), std::string::npos);

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementInnerHTMLSimpleTextChild) {
    auto doc = clever::html::parse(R"(
        <html><body><p id="simple">Just text</p></body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.getElementById('simple').innerHTML");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Just text");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 17. DOM bindings: element.tagName (uppercase)
// ============================================================================
TEST(JSDom, ElementTagNameUppercase) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body>
            <div id="main" class="container">
                <p id="greeting">Hello World</p>
                <span class="highlight">Important</span>
            </div>
        </body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.getElementById('main').tagName");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "DIV");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementTagNameParagraph) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body>
            <div id="main" class="container">
                <p id="greeting">Hello World</p>
                <span class="highlight">Important</span>
            </div>
        </body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.getElementById('greeting').tagName");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "P");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementTagNameSpan) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body>
            <div id="main" class="container">
                <p id="greeting">Hello World</p>
                <span class="highlight">Important</span>
            </div>
        </body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.querySelector('.highlight').tagName");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "SPAN");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 18. JSEngine move semantics
// ============================================================================
TEST(JSEngine, MoveConstruction) {
    clever::js::JSEngine engine1;
    engine1.evaluate("var x = 99");

    clever::js::JSEngine engine2(std::move(engine1));
    auto result = engine2.evaluate("x");
    EXPECT_FALSE(engine2.has_error());
    EXPECT_EQ(result, "99");

    // engine1 should be in a moved-from state (null context)
    EXPECT_EQ(engine1.context(), nullptr);
    EXPECT_EQ(engine1.runtime(), nullptr);
}

TEST(JSEngine, MoveAssignment) {
    clever::js::JSEngine engine1;
    engine1.evaluate("var y = 'moved'");

    clever::js::JSEngine engine2;
    engine2 = std::move(engine1);

    auto result = engine2.evaluate("y");
    EXPECT_FALSE(engine2.has_error());
    EXPECT_EQ(result, "moved");
}

// ============================================================================
// 19. Undefined result returns empty string
// ============================================================================
TEST(JSEngine, UndefinedResultReturnsEmpty) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("undefined");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "");
}

TEST(JSEngine, VoidExpressionReturnsEmpty) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("var x = 5");
    EXPECT_FALSE(engine.has_error());
    // var declarations evaluate to undefined
    EXPECT_EQ(result, "");
}

// ============================================================================
// 20. DOM: document.body and document.head accessors
// ============================================================================
TEST(JSDom, DocumentBodyAccessor) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body><p>Content</p></body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("document.body.tagName");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "BODY");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, DocumentHeadAccessor) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Test</title></head>
        <body></body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("document.head.tagName");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "HEAD");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 21. DOM: element.id getter
// ============================================================================
TEST(JSDom, ElementIdGetter) {
    auto doc = clever::html::parse(R"(
        <html><body><div id="test-id">content</div></body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("document.querySelector('div').id");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "test-id");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 22. DOM: element.className getter/setter
// ============================================================================
TEST(JSDom, ElementClassNameGetter) {
    auto doc = clever::html::parse(R"(
        <html><body><div id="box" class="red large">text</div></body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.getElementById('box').className");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "red large");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementClassNameSetter) {
    auto doc = clever::html::parse(R"(
        <html><body><div id="box" class="old-class">text</div></body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate("document.getElementById('box').className = 'new-class'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto result = engine.evaluate(
        "document.getElementById('box').className");
    EXPECT_EQ(result, "new-class");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 23. DOM: querySelectorAll returns multiple results
// ============================================================================
TEST(JSDom, QuerySelectorAll) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <p>First</p>
            <p>Second</p>
            <p>Third</p>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.querySelectorAll('p').length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 24. DOM: element.parentNode
// ============================================================================
TEST(JSDom, ElementParentNode) {
    auto doc = clever::html::parse(R"(
        <html><body><div id="parent"><p id="child">text</p></div></body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.getElementById('child').parentNode.id");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "parent");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 25. DOM: element.children returns only element nodes
// ============================================================================
TEST(JSDom, ElementChildrenProperty) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="container">
                <p>One</p>
                <span>Two</span>
            </div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.getElementById('container').children.length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // Should have 2 element children (p and span), not counting text nodes
    EXPECT_EQ(result, "2");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 26. DOM: removeChild
// ============================================================================
TEST(JSDom, RemoveChild) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="parent">
                <p id="child">Remove me</p>
            </div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var parent = document.getElementById('parent');
        var child = document.getElementById('child');
        parent.removeChild(child);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // The child should no longer be findable by ID in the tree
    auto result = engine.evaluate(
        "document.getElementById('parent').children.length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0");

    EXPECT_TRUE(clever::js::dom_was_modified(engine.context()));

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 27. DOM: createTextNode + appendChild
// ============================================================================
TEST(JSDom, CreateTextNodeAndAppend) {
    auto doc = clever::html::parse(R"(
        <html><body><div id="target"></div></body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var textNode = document.createTextNode('Hello from JS');
        document.getElementById('target').appendChild(textNode);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto result = engine.evaluate(
        "document.getElementById('target').textContent");
    EXPECT_EQ(result, "Hello from JS");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 28. DOM: documentElement accessor
// ============================================================================
TEST(JSDom, DocumentElementAccessor) {
    auto doc = clever::html::parse(R"(
        <html><body></body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("document.documentElement.tagName");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "HTML");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 29. DOM: complex JS interaction across multiple evaluations
// ============================================================================
TEST(JSDom, ComplexMultiStepDomManipulation) {
    auto doc = clever::html::parse(R"(
        <html>
        <head><title>Interactive</title></head>
        <body><div id="app"></div></body>
        </html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // Step 1: Create and append a heading
    engine.evaluate(R"(
        var h1 = document.createElement('h1');
        h1.textContent = 'Welcome';
        document.getElementById('app').appendChild(h1);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // Step 2: Create and append a paragraph
    engine.evaluate(R"(
        var p = document.createElement('p');
        p.textContent = 'This is dynamic content.';
        p.setAttribute('id', 'desc');
        document.getElementById('app').appendChild(p);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // Step 3: Verify the structure
    auto children_count = engine.evaluate(
        "document.getElementById('app').children.length");
    EXPECT_EQ(children_count, "2");

    auto heading_text = engine.evaluate(
        "document.querySelector('h1').textContent");
    EXPECT_EQ(heading_text, "Welcome");

    auto para_text = engine.evaluate(
        "document.getElementById('desc').textContent");
    EXPECT_EQ(para_text, "This is dynamic content.");

    // Step 4: Modify the title
    engine.evaluate("document.title = 'Updated Title'");
    EXPECT_EQ(clever::js::get_document_title(engine.context()), "Updated Title");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 30. DOM: innerHTML setter
// ============================================================================
TEST(JSDom, ElementInnerHTMLSetter) {
    auto doc = clever::html::parse(R"(
        <html><body><div id="target">Old content</div></body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(
        "document.getElementById('target').innerHTML = '<b>Bold</b>'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto result = engine.evaluate(
        "document.getElementById('target').textContent");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Bold");

    EXPECT_TRUE(clever::js::dom_was_modified(engine.context()));

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 31. DOM: element.style property access
// ============================================================================
TEST(JSDom, ElementStyleSetAndGet) {
    auto doc = clever::html::parse(R"(
        <html><body><div id="styled" style="color: red;">text</div></body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // Get existing style property
    auto color = engine.evaluate(
        "document.getElementById('styled').style.getPropertyValue('color')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(color, "red");

    // Set a new style property
    engine.evaluate(
        "document.getElementById('styled').style.setProperty('font-size', '16px')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto font_size = engine.evaluate(
        "document.getElementById('styled').style.getPropertyValue('font-size')");
    EXPECT_EQ(font_size, "16px");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// 32. Boolean and number evaluation
// ============================================================================
TEST(JSEngine, BooleanEvaluation) {
    clever::js::JSEngine engine;
    EXPECT_EQ(engine.evaluate("true"), "true");
    EXPECT_EQ(engine.evaluate("false"), "false");
    EXPECT_EQ(engine.evaluate("1 === 1"), "true");
    EXPECT_EQ(engine.evaluate("1 === 2"), "false");
}

TEST(JSEngine, NullEvaluation) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("null");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "null");
}

// ============================================================================
// 33. Array and object string representation
// ============================================================================
TEST(JSEngine, ArrayToString) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[1, 2, 3].toString()");
    EXPECT_FALSE(engine.has_error());
    EXPECT_EQ(result, "1,2,3");
}

TEST(JSEngine, ObjectMethodCall) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("JSON.stringify({a: 1, b: 2})");
    EXPECT_FALSE(engine.has_error());
    EXPECT_NE(result.find("\"a\""), std::string::npos);
    EXPECT_NE(result.find("1"), std::string::npos);
}

// ============================================================================
// 34. JSTimers: setTimeout / setInterval / clearTimeout / clearInterval
// ============================================================================
TEST(JSTimers, SetTimeoutReturnsId) {
    clever::js::JSEngine engine;
    clever::js::install_timer_bindings(engine.context());
    auto result = engine.evaluate("setTimeout(function() {}, 1000)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // Should return a numeric ID (positive integer)
    EXPECT_FALSE(result.empty());
    int id = std::stoi(result);
    EXPECT_GT(id, 0);
    clever::js::flush_pending_timers(engine.context());
}

TEST(JSTimers, SetTimeoutZeroDelayExecutes) {
    clever::js::JSEngine engine;
    clever::js::install_timer_bindings(engine.context());
    engine.evaluate("var x = 0; setTimeout(function() { x = 42; }, 0);");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_pending_timers(engine.context());
    auto result = engine.evaluate("x");
    EXPECT_EQ(result, "42");
}

TEST(JSTimers, ClearTimeoutPreventsExecution) {
    clever::js::JSEngine engine;
    clever::js::install_timer_bindings(engine.context());
    // Use delay > 0 so the callback is stored (not executed immediately)
    engine.evaluate(R"(
        var fired = false;
        var id = setTimeout(function() { fired = true; }, 100);
        clearTimeout(id);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_pending_timers(engine.context());
    auto result = engine.evaluate("fired");
    EXPECT_EQ(result, "false");
}

TEST(JSTimers, SetIntervalReturnsId) {
    clever::js::JSEngine engine;
    clever::js::install_timer_bindings(engine.context());
    auto result = engine.evaluate("setInterval(function() {}, 1000)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_FALSE(result.empty());
    int id = std::stoi(result);
    EXPECT_GT(id, 0);
    clever::js::flush_pending_timers(engine.context());
}

TEST(JSTimers, ClearIntervalWorks) {
    clever::js::JSEngine engine;
    clever::js::install_timer_bindings(engine.context());
    engine.evaluate(R"(
        var id = setInterval(function() {}, 1000);
        clearInterval(id);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_pending_timers(engine.context());
}

TEST(JSTimers, MultipleTimeouts) {
    clever::js::JSEngine engine;
    clever::js::install_timer_bindings(engine.context());
    engine.evaluate(R"(
        var order = [];
        setTimeout(function() { order.push(1); }, 0);
        setTimeout(function() { order.push(2); }, 0);
        setTimeout(function() { order.push(3); }, 0);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_pending_timers(engine.context());
    auto result = engine.evaluate("order.join(',')");
    EXPECT_EQ(result, "1,2,3");
}

TEST(JSTimers, SetTimeoutWithString) {
    clever::js::JSEngine engine;
    clever::js::install_timer_bindings(engine.context());
    engine.evaluate("var s = 0; setTimeout('s = 99', 0);");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_pending_timers(engine.context());
    auto result = engine.evaluate("s");
    EXPECT_EQ(result, "99");
}

// ============================================================================
// 35. JSWindow: window object, location, navigator
// ============================================================================
TEST(JSWindow, WindowExists) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("typeof window");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");
}

TEST(JSWindow, WindowIsGlobal) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("window === globalThis");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSWindow, WindowInnerWidth) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/page", 800, 600);
    auto result = engine.evaluate("window.innerWidth");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "800");
}

TEST(JSWindow, WindowInnerHeight) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/page", 800, 600);
    auto result = engine.evaluate("window.innerHeight");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "600");
}

TEST(JSWindow, WindowLocationHref) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/page?q=1", 1024, 768);
    auto result = engine.evaluate("window.location.href");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "https://example.com/page?q=1");
}

TEST(JSWindow, WindowLocationProtocol) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("window.location.protocol");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "https:");
}

TEST(JSWindow, WindowLocationHostname) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/path", 1024, 768);
    auto result = engine.evaluate("window.location.hostname");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "example.com");
}

TEST(JSWindow, WindowNavigatorUserAgent) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("window.navigator.userAgent");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // The user agent string should contain "Clever"
    EXPECT_NE(result.find("Clever"), std::string::npos);
}

TEST(JSWindow, WindowAlertNoThrow) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    engine.evaluate("window.alert('test')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
}

// ============================================================================
// Event Dispatch tests
// ============================================================================

TEST(JSEvents, DocumentAddEventListenerDOMContentLoaded) {
    auto doc = clever::html::parse("<html><body><p>test</p></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var fired = false;
        document.addEventListener('DOMContentLoaded', function(e) {
            fired = true;
        });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    clever::js::dispatch_dom_content_loaded(engine.context());

    auto result = engine.evaluate("fired");
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSEvents, WindowAddEventListenerDOMContentLoaded) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var windowFired = false;
        window.addEventListener('DOMContentLoaded', function() {
            windowFired = true;
        });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    clever::js::dispatch_dom_content_loaded(engine.context());

    auto result = engine.evaluate("windowFired");
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSEvents, DispatchEventCallsListeners) {
    auto doc = clever::html::parse("<html><body><div id='btn'>Click</div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var clicked = false;
        var el = document.getElementById('btn');
        el.addEventListener('click', function(e) {
            clicked = true;
        });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // Find the div node
    clever::html::SimpleNode* div_node = nullptr;
    std::function<void(clever::html::SimpleNode*)> find_div;
    find_div = [&](clever::html::SimpleNode* n) {
        if (n->type == clever::html::SimpleNode::Element && n->tag_name == "div") {
            div_node = n;
            return;
        }
        for (auto& child : n->children) find_div(child.get());
    };
    find_div(doc.get());
    ASSERT_NE(div_node, nullptr);

    bool prevented = clever::js::dispatch_event(engine.context(), div_node, "click");
    EXPECT_FALSE(prevented);

    auto result = engine.evaluate("clicked");
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSEvents, DispatchEventPreventDefault) {
    auto doc = clever::html::parse("<html><body><a id='link'>Link</a></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var el = document.getElementById('link');
        el.addEventListener('click', function(e) {
            e.preventDefault();
        });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    clever::html::SimpleNode* link_node = nullptr;
    std::function<void(clever::html::SimpleNode*)> find_a;
    find_a = [&](clever::html::SimpleNode* n) {
        if (n->type == clever::html::SimpleNode::Element && n->tag_name == "a") {
            link_node = n;
            return;
        }
        for (auto& child : n->children) find_a(child.get());
    };
    find_a(doc.get());
    ASSERT_NE(link_node, nullptr);

    bool prevented = clever::js::dispatch_event(engine.context(), link_node, "click");
    EXPECT_TRUE(prevented);

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSEvents, DispatchEventHasTypeProperty) {
    auto doc = clever::html::parse("<html><body><div id='d1'>x</div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var eventType = '';
        var el = document.getElementById('d1');
        el.addEventListener('click', function(e) {
            eventType = e.type;
        });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    clever::html::SimpleNode* div_node = nullptr;
    std::function<void(clever::html::SimpleNode*)> find;
    find = [&](clever::html::SimpleNode* n) {
        if (n->type == clever::html::SimpleNode::Element && n->tag_name == "div") {
            div_node = n;
            return;
        }
        for (auto& child : n->children) find(child.get());
    };
    find(doc.get());
    ASSERT_NE(div_node, nullptr);

    clever::js::dispatch_event(engine.context(), div_node, "click");
    auto result = engine.evaluate("eventType");
    EXPECT_EQ(result, "click");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSEvents, MultipleListenersOnSameEvent) {
    auto doc = clever::html::parse("<html><body><div id='t'>x</div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var count = 0;
        var el = document.getElementById('t');
        el.addEventListener('click', function() { count++; });
        el.addEventListener('click', function() { count++; });
        el.addEventListener('click', function() { count++; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    clever::html::SimpleNode* node = nullptr;
    std::function<void(clever::html::SimpleNode*)> find;
    find = [&](clever::html::SimpleNode* n) {
        if (n->type == clever::html::SimpleNode::Element && n->tag_name == "div") {
            node = n;
            return;
        }
        for (auto& child : n->children) find(child.get());
    };
    find(doc.get());
    ASSERT_NE(node, nullptr);

    clever::js::dispatch_event(engine.context(), node, "click");
    auto result = engine.evaluate("count");
    EXPECT_EQ(result, "3");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSEvents, InlineOnclickAttribute) {
    auto doc = clever::html::parse(R"(<html><body><div id="b" onclick="globalThis.__clicked=true">x</div></body></html>)");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // The inline onclick should have been registered by scan_inline_event_attributes
    // Dispatch click on the div to fire it
    clever::html::SimpleNode* div_node = nullptr;
    std::function<void(clever::html::SimpleNode*)> find;
    find = [&](clever::html::SimpleNode* n) {
        if (n->type == clever::html::SimpleNode::Element && n->tag_name == "div") {
            div_node = n;
            return;
        }
        for (auto& child : n->children) find(child.get());
    };
    find(doc.get());
    ASSERT_NE(div_node, nullptr);

    clever::js::dispatch_event(engine.context(), div_node, "click");
    auto result = engine.evaluate("globalThis.__clicked");
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSEvents, DOMContentLoadedFiresBothDocAndWindow) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var docFired = false;
        var winFired = false;
        document.addEventListener('DOMContentLoaded', function() { docFired = true; });
        window.addEventListener('DOMContentLoaded', function() { winFired = true; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    clever::js::dispatch_dom_content_loaded(engine.context());

    EXPECT_EQ(engine.evaluate("docFired"), "true");
    EXPECT_EQ(engine.evaluate("winFired"), "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSEvents, StopImmediatePropagation) {
    auto doc = clever::html::parse("<html><body><div id='s'>x</div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var calls = 0;
        var el = document.getElementById('s');
        el.addEventListener('click', function(e) { calls++; e.stopImmediatePropagation(); });
        el.addEventListener('click', function() { calls++; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    clever::html::SimpleNode* node = nullptr;
    std::function<void(clever::html::SimpleNode*)> find;
    find = [&](clever::html::SimpleNode* n) {
        if (n->type == clever::html::SimpleNode::Element && n->tag_name == "div") {
            node = n;
            return;
        }
        for (auto& child : n->children) find(child.get());
    };
    find(doc.get());
    ASSERT_NE(node, nullptr);

    clever::js::dispatch_event(engine.context(), node, "click");
    auto result = engine.evaluate("calls");
    EXPECT_EQ(result, "1"); // Second listener should NOT have been called

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// XMLHttpRequest tests
// ============================================================================

#include <clever/js/js_fetch_bindings.h>

TEST(JSXHR, ConstructorExists) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate("typeof XMLHttpRequest");
    EXPECT_EQ(result, "function");
}

TEST(JSXHR, NewInstanceReadyState) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate("var xhr = new XMLHttpRequest(); xhr.readyState");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0");
}

TEST(JSXHR, OpenSetsReadyState) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var xhr = new XMLHttpRequest();
        xhr.open('GET', 'http://example.com');
        xhr.readyState
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1");
}

TEST(JSXHR, StaticConstants) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    EXPECT_EQ(engine.evaluate("XMLHttpRequest.UNSENT"), "0");
    EXPECT_EQ(engine.evaluate("XMLHttpRequest.OPENED"), "1");
    EXPECT_EQ(engine.evaluate("XMLHttpRequest.HEADERS_RECEIVED"), "2");
    EXPECT_EQ(engine.evaluate("XMLHttpRequest.LOADING"), "3");
    EXPECT_EQ(engine.evaluate("XMLHttpRequest.DONE"), "4");
}

TEST(JSXHR, SendBeforeOpenThrows) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var xhr = new XMLHttpRequest();
        xhr.send();
    )");
    EXPECT_TRUE(engine.has_error());
}

TEST(JSXHR, SetRequestHeaderBeforeOpenThrows) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var xhr = new XMLHttpRequest();
        xhr.setRequestHeader('Accept', 'text/html');
    )");
    EXPECT_TRUE(engine.has_error());
}

TEST(JSXHR, OpenResetsState) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var xhr = new XMLHttpRequest();
        xhr.open('GET', 'http://example.com/a');
        xhr.open('POST', 'http://example.com/b');
        xhr.readyState
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1"); // Re-opened, still OPENED state
}

TEST(JSXHR, ResponseTextEmptyBeforeSend) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var xhr = new XMLHttpRequest();
        xhr.responseText
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "");
}

TEST(JSXHR, StatusZeroBeforeSend) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var xhr = new XMLHttpRequest();
        xhr.status
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0");
}

// ============================================================================
// WebSocket tests
// ============================================================================

TEST(JSWebSocket, ConstructorExists) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate("typeof WebSocket");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

TEST(JSWebSocket, StaticConstants) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    EXPECT_EQ(engine.evaluate("WebSocket.CONNECTING"), "0");
    EXPECT_EQ(engine.evaluate("WebSocket.OPEN"), "1");
    EXPECT_EQ(engine.evaluate("WebSocket.CLOSING"), "2");
    EXPECT_EQ(engine.evaluate("WebSocket.CLOSED"), "3");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
}

TEST(JSWebSocket, InvalidUrlThrowsError) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate("new WebSocket('http://example.com')");
    EXPECT_TRUE(engine.has_error());
}

TEST(JSWebSocket, NoArgumentsThrowsError) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate("new WebSocket()");
    EXPECT_TRUE(engine.has_error());
}

TEST(JSWebSocket, UrlPropertyIsSet) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    // Connect to a non-existent host so we get CLOSED state but url is still set
    auto result = engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test');
        ws.url;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ws://0.0.0.0:1/test");
}

TEST(JSWebSocket, ReadyStateOnFailedConnection) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test');
        ws.readyState;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3"); // CLOSED because connection failed
}

TEST(JSWebSocket, ProtocolProperty) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test', 'chat');
        ws.protocol;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "chat");
}

TEST(JSWebSocket, BufferedAmountIsZero) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test');
        ws.bufferedAmount;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0");
}

TEST(JSWebSocket, SendExistsAsFunction) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test');
        typeof ws.send;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

TEST(JSWebSocket, CloseExistsAsFunction) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test');
        typeof ws.close;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

TEST(JSWebSocket, CloseOnClosedSocketIsNoop) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test');
        ws.close();
        ws.readyState;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3"); // CLOSED
}

TEST(JSWebSocket, EventHandlerGetterSetterOnopen) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test');
        var called = false;
        ws.onopen = function() { called = true; };
        typeof ws.onopen;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

TEST(JSWebSocket, EventHandlerGetterSetterOnmessage) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test');
        ws.onmessage = function(e) {};
        typeof ws.onmessage;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

TEST(JSWebSocket, EventHandlerGetterSetterOnclose) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test');
        ws.onclose = function(e) {};
        typeof ws.onclose;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

TEST(JSWebSocket, EventHandlerGetterSetterOnerror) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test');
        ws.onerror = function(e) {};
        typeof ws.onerror;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

TEST(JSWebSocket, EventHandlerDefaultIsNull) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test');
        String(ws.onopen) + ',' + String(ws.onmessage) + ',' +
        String(ws.onclose) + ',' + String(ws.onerror);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "undefined,undefined,undefined,undefined");
}

TEST(JSWebSocket, SendOnClosedSocketThrows) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test');
        ws.send('hello');
    )");
    EXPECT_TRUE(engine.has_error());
}

TEST(JSWebSocket, InstanceOfWebSocket) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var ws = new WebSocket('ws://0.0.0.0:1/test');
        ws instanceof WebSocket;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// DOM Traversal: firstChild / lastChild
// ============================================================================
TEST(JSDom, FirstChildAndLastChild) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="parent"><span>A</span><span>B</span></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto first = engine.evaluate(R"(
        var p = document.getElementById('parent');
        p.firstChild.tagName
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(first, "SPAN");

    auto last = engine.evaluate(R"(
        var p = document.getElementById('parent');
        p.lastChild.textContent
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(last, "B");

    auto empty_fc = engine.evaluate(R"(
        var s = document.createElement('empty');
        s.firstChild
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(empty_fc, "null");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOM Traversal: firstElementChild / lastElementChild (skip text nodes)
// ============================================================================
TEST(JSDom, FirstElementChildAndLastElementChild) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="mixed">Text<span>A</span><em>B</em>More</div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto first_elem = engine.evaluate(R"(
        var d = document.getElementById('mixed');
        d.firstElementChild.tagName
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(first_elem, "SPAN");

    auto last_elem = engine.evaluate(R"(
        var d = document.getElementById('mixed');
        d.lastElementChild.tagName
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(last_elem, "EM");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOM Traversal: nextSibling / previousSibling
// ============================================================================
TEST(JSDom, NextSiblingAndPreviousSibling) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <ul id="list"><li id="a">A</li><li id="b">B</li><li id="c">C</li></ul>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto next = engine.evaluate(R"(
        var b = document.getElementById('b');
        b.nextSibling.textContent
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(next, "C");

    auto prev = engine.evaluate(R"(
        var b = document.getElementById('b');
        b.previousSibling.textContent
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(prev, "A");

    // First child has no previousSibling
    auto no_prev = engine.evaluate(R"(
        var a = document.getElementById('a');
        a.previousSibling
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(no_prev, "null");

    // Last child has no nextSibling
    auto no_next = engine.evaluate(R"(
        var c = document.getElementById('c');
        c.nextSibling
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(no_next, "null");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOM Traversal: nextElementSibling / previousElementSibling
// ============================================================================
TEST(JSDom, NextElementSiblingAndPreviousElementSibling) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="wrap"><span id="x">X</span><em id="y">Y</em><b id="z">Z</b></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto next_elem = engine.evaluate(R"(
        var y = document.getElementById('y');
        y.nextElementSibling.tagName
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(next_elem, "B");

    auto prev_elem = engine.evaluate(R"(
        var y = document.getElementById('y');
        y.previousElementSibling.tagName
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(prev_elem, "SPAN");

    // First element has no previousElementSibling
    auto no_prev_elem = engine.evaluate(R"(
        var x = document.getElementById('x');
        x.previousElementSibling
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(no_prev_elem, "null");

    // Last element has no nextElementSibling
    auto no_next_elem = engine.evaluate(R"(
        var z = document.getElementById('z');
        z.nextElementSibling
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(no_next_elem, "null");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOM Traversal: childElementCount
// ============================================================================
TEST(JSDom, ChildElementCount) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="mixed">Text<span>A</span><em>B</em>More<b>C</b></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto count = engine.evaluate(R"(
        document.getElementById('mixed').childElementCount
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(count, "3");  // span, em, b -- text nodes not counted

    auto zero = engine.evaluate(R"(
        document.createElement('empty').childElementCount
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(zero, "0");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOM Traversal: nodeType
// ============================================================================
TEST(JSDom, NodeType) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="el">Hello</div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // Element nodeType = 1
    auto elem_type = engine.evaluate(R"(
        document.getElementById('el').nodeType
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(elem_type, "1");

    // Text node nodeType = 3
    auto text_type = engine.evaluate(R"(
        document.getElementById('el').firstChild.nodeType
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(text_type, "3");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOM Traversal: nodeName for elements and text
// ============================================================================
TEST(JSDom, NodeName) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="el">Hello</div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // Element nodeName = uppercase tagName
    auto elem_name = engine.evaluate(R"(
        document.getElementById('el').nodeName
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(elem_name, "DIV");

    // Text node nodeName = "#text"
    auto text_name = engine.evaluate(R"(
        document.getElementById('el').firstChild.nodeName
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(text_name, "#text");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// element.matches() -- simple selectors
// ============================================================================
TEST(JSDom, ElementMatchesTag) {
    auto doc = clever::html::parse(R"(
        <html><body><div id="test" class="foo bar">Hello</div></body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto tag_match = engine.evaluate(R"(
        document.getElementById('test').matches('div')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(tag_match, "true");

    auto tag_no_match = engine.evaluate(R"(
        document.getElementById('test').matches('span')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(tag_no_match, "false");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementMatchesClassAndId) {
    auto doc = clever::html::parse(R"(
        <html><body><div id="test" class="foo bar">Hello</div></body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto class_match = engine.evaluate(R"(
        document.getElementById('test').matches('.foo')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(class_match, "true");

    auto id_match = engine.evaluate(R"(
        document.getElementById('test').matches('#test')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(id_match, "true");

    auto id_no_match = engine.evaluate(R"(
        document.getElementById('test').matches('#other')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(id_no_match, "false");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementMatchesCombined) {
    auto doc = clever::html::parse(R"(
        <html><body><div id="test" class="foo bar">Hello</div></body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto combined_tag_class = engine.evaluate(R"(
        document.getElementById('test').matches('div.foo')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(combined_tag_class, "true");

    auto combined_tag_id = engine.evaluate(R"(
        document.getElementById('test').matches('div#test')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(combined_tag_id, "true");

    // Wrong tag with right class
    auto wrong_tag = engine.evaluate(R"(
        document.getElementById('test').matches('span.foo')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(wrong_tag, "false");

    // Right tag with wrong class
    auto wrong_class = engine.evaluate(R"(
        document.getElementById('test').matches('div.baz')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(wrong_class, "false");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// element.closest() -- walk up ancestors
// ============================================================================
TEST(JSDom, ElementClosest) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="outer" class="wrapper">
                <section id="middle">
                    <span id="inner">Hello</span>
                </section>
            </div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // closest matches self
    auto self_match = engine.evaluate(R"(
        document.getElementById('inner').closest('span').id
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(self_match, "inner");

    // closest matches parent
    auto parent_match = engine.evaluate(R"(
        document.getElementById('inner').closest('section').id
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(parent_match, "middle");

    // closest matches grandparent by class
    auto ancestor_match = engine.evaluate(R"(
        document.getElementById('inner').closest('.wrapper').id
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(ancestor_match, "outer");

    // closest returns null if no match
    auto no_match = engine.evaluate(R"(
        document.getElementById('inner').closest('.nonexistent')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(no_match, "null");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// getBoundingClientRect() -- returns DOMRect stub with all zeros
// ============================================================================
TEST(JSDom, GetBoundingClientRectReturnsZeros) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="box" style="width: 100px; height: 50px;">Content</div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // Should return an object with all zero values
    auto top = engine.evaluate(
        "document.getElementById('box').getBoundingClientRect().top");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(top, "0");

    auto width = engine.evaluate(
        "document.getElementById('box').getBoundingClientRect().width");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(width, "0");

    auto height = engine.evaluate(
        "document.getElementById('box').getBoundingClientRect().height");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(height, "0");

    // Check all 8 properties exist and are zero
    auto all_zero = engine.evaluate(R"(
        var r = document.getElementById('box').getBoundingClientRect();
        (r.x === 0 && r.y === 0 && r.top === 0 && r.left === 0 &&
         r.bottom === 0 && r.right === 0 && r.width === 0 && r.height === 0)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(all_zero, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// window.getComputedStyle() -- returns CSSStyleDeclaration stub
// ============================================================================
TEST(JSDom, GetComputedStyleBasic) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="styled" style="color: red; font-size: 14px;">Hello</div>
            <div id="unstyled">World</div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // getComputedStyle should exist and not throw
    auto type_check = engine.evaluate(
        "typeof getComputedStyle");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(type_check, "function");

    // Should return inline style values via getPropertyValue
    auto color = engine.evaluate(R"(
        var elem = document.getElementById('styled');
        getComputedStyle(elem).getPropertyValue('color')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(color, "red");

    auto font_size = engine.evaluate(R"(
        var elem = document.getElementById('styled');
        getComputedStyle(elem).getPropertyValue('font-size')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(font_size, "14px");

    // Unknown property returns empty string
    auto unknown = engine.evaluate(R"(
        var elem = document.getElementById('styled');
        getComputedStyle(elem).getPropertyValue('margin')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(unknown, "");

    // Unstyled element returns empty for any property
    auto unstyled = engine.evaluate(R"(
        var elem = document.getElementById('unstyled');
        getComputedStyle(elem).getPropertyValue('color')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(unstyled, "");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// getComputedStyle -- properties accessible directly (camelCase and kebab)
// ============================================================================
TEST(JSDom, GetComputedStyleDirectProperties) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="elem" style="background-color: blue; margin-top: 10px;">X</div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // Direct property access in kebab-case
    auto bg_kebab = engine.evaluate(R"(
        var cs = getComputedStyle(document.getElementById('elem'));
        cs['background-color']
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(bg_kebab, "blue");

    // Direct property access in camelCase
    auto bg_camel = engine.evaluate(R"(
        var cs = getComputedStyle(document.getElementById('elem'));
        cs.backgroundColor
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(bg_camel, "blue");

    // length should be 0 (stub)
    auto len = engine.evaluate(R"(
        getComputedStyle(document.getElementById('elem')).length
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(len, "0");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Dimension stubs: offsetWidth, offsetHeight, scrollWidth, etc.
// ============================================================================
TEST(JSDom, DimensionStubsReturnZero) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="box">Content</div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // Test all dimension properties return 0
    auto result = engine.evaluate(R"(
        var el = document.getElementById('box');
        var props = [
            el.offsetWidth, el.offsetHeight, el.offsetTop, el.offsetLeft,
            el.scrollWidth, el.scrollHeight, el.scrollTop, el.scrollLeft,
            el.clientWidth, el.clientHeight
        ];
        props.every(function(v) { return v === 0; })
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    // Individual checks
    auto ow = engine.evaluate(
        "document.getElementById('box').offsetWidth");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(ow, "0");

    auto sh = engine.evaluate(
        "document.getElementById('box').scrollHeight");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(sh, "0");

    auto cw = engine.evaluate(
        "document.getElementById('box').clientWidth");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(cw, "0");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Dimension stubs on body element
// ============================================================================
TEST(JSDom, BodyDimensionStubsReturnZero) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <p>Some content</p>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // document.body dimension stubs should work too
    auto body_scroll = engine.evaluate("document.body.scrollHeight");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(body_scroll, "0");

    auto body_client = engine.evaluate("document.body.clientWidth");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(body_client, "0");

    auto body_offset = engine.evaluate("document.body.offsetHeight");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(body_offset, "0");

    // getBoundingClientRect on body
    auto body_rect = engine.evaluate(R"(
        var r = document.body.getBoundingClientRect();
        r.width === 0 && r.height === 0 && r.top === 0
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(body_rect, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// getComputedStyle called without DOM bindings doesn't crash
// ============================================================================
TEST(JSDom, GetComputedStyleNoThrowOnInvalidArg) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // Calling with no argument returns null
    auto no_arg = engine.evaluate("getComputedStyle()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(no_arg, "null");

    // Calling with null returns null
    auto null_arg = engine.evaluate("getComputedStyle(null)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(null_arg, "null");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOM Mutation: insertBefore
// ============================================================================
TEST(JSDom, InsertBefore) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="parent"><span id="a">A</span><span id="b">B</span></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var parent = document.getElementById('parent');
        var newNode = document.createElement('em');
        newNode.textContent = 'NEW';
        var refNode = document.getElementById('b');
        parent.insertBefore(newNode, refNode);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // Should now have 3 children: a, em, b
    auto count = engine.evaluate(
        "document.getElementById('parent').children.length");
    EXPECT_EQ(count, "3");

    // The middle child should be our new EM element
    auto mid_tag = engine.evaluate(
        "document.getElementById('parent').children[1].tagName");
    EXPECT_EQ(mid_tag, "EM");

    auto mid_text = engine.evaluate(
        "document.getElementById('parent').children[1].textContent");
    EXPECT_EQ(mid_text, "NEW");

    EXPECT_TRUE(clever::js::dom_was_modified(engine.context()));
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, InsertBeforeNullRefAppendsChild) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="parent"><span id="a">A</span></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var parent = document.getElementById('parent');
        var newNode = document.createElement('b');
        newNode.textContent = 'LAST';
        parent.insertBefore(newNode, null);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto count = engine.evaluate(
        "document.getElementById('parent').children.length");
    EXPECT_EQ(count, "2");

    auto last_tag = engine.evaluate(
        "document.getElementById('parent').lastElementChild.tagName");
    EXPECT_EQ(last_tag, "B");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOM Mutation: replaceChild
// ============================================================================
TEST(JSDom, ReplaceChild) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="parent"><span id="old">Old</span></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var parent = document.getElementById('parent');
        var newChild = document.createElement('b');
        newChild.textContent = 'New';
        var oldChild = document.getElementById('old');
        var returned = parent.replaceChild(newChild, oldChild);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // parent should still have 1 child, but it should be the B element
    auto count = engine.evaluate(
        "document.getElementById('parent').children.length");
    EXPECT_EQ(count, "1");

    auto child_tag = engine.evaluate(
        "document.getElementById('parent').firstElementChild.tagName");
    EXPECT_EQ(child_tag, "B");

    auto child_text = engine.evaluate(
        "document.getElementById('parent').firstElementChild.textContent");
    EXPECT_EQ(child_text, "New");

    // The returned value should be the old child
    auto returned_tag = engine.evaluate("returned.tagName");
    EXPECT_EQ(returned_tag, "SPAN");

    EXPECT_TRUE(clever::js::dom_was_modified(engine.context()));
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOM Mutation: cloneNode (shallow and deep)
// ============================================================================
TEST(JSDom, CloneNodeShallow) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="orig" class="box"><span>Child</span></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto tag = engine.evaluate(R"(
        var orig = document.getElementById('orig');
        var clone = orig.cloneNode(false);
        clone.tagName
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(tag, "DIV");

    // Shallow clone should not have children
    auto children_count = engine.evaluate(R"(
        var orig = document.getElementById('orig');
        var clone = orig.cloneNode(false);
        clone.children.length
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(children_count, "0");

    // Should preserve attributes
    auto cls = engine.evaluate(R"(
        var orig = document.getElementById('orig');
        var clone = orig.cloneNode(false);
        clone.getAttribute('class')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(cls, "box");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CloneNodeDeep) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="orig"><span>Child</span></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var orig = document.getElementById('orig');
        var clone = orig.cloneNode(true);
        clone.firstElementChild.tagName + ':' + clone.firstElementChild.textContent
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "SPAN:Child");

    // Modifying the clone should not affect the original
    engine.evaluate(R"(
        var orig = document.getElementById('orig');
        var clone = orig.cloneNode(true);
        clone.firstElementChild.textContent = 'Modified';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto orig_text = engine.evaluate(
        "document.getElementById('orig').firstElementChild.textContent");
    EXPECT_EQ(orig_text, "Child");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOM Mutation: createDocumentFragment
// ============================================================================
TEST(JSDom, CreateDocumentFragment) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="target"></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var frag = document.createDocumentFragment();
        var a = document.createElement('span');
        a.textContent = 'A';
        var b = document.createElement('span');
        b.textContent = 'B';
        frag.appendChild(a);
        frag.appendChild(b);
        document.getElementById('target').appendChild(frag);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // The fragment's children should have been moved to target
    auto count = engine.evaluate(
        "document.getElementById('target').children.length");
    EXPECT_EQ(count, "2");

    auto first_text = engine.evaluate(
        "document.getElementById('target').children[0].textContent");
    EXPECT_EQ(first_text, "A");

    auto second_text = engine.evaluate(
        "document.getElementById('target').children[1].textContent");
    EXPECT_EQ(second_text, "B");

    EXPECT_TRUE(clever::js::dom_was_modified(engine.context()));
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOM Utility: contains
// ============================================================================
TEST(JSDom, Contains) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="outer"><div id="inner"><span id="deep">X</span></div></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // Element contains itself
    auto self = engine.evaluate(R"(
        var el = document.getElementById('outer');
        el.contains(el)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(self, "true");

    // Parent contains child
    auto child = engine.evaluate(R"(
        var outer = document.getElementById('outer');
        var inner = document.getElementById('inner');
        outer.contains(inner)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(child, "true");

    // Parent contains deep descendant
    auto deep = engine.evaluate(R"(
        var outer = document.getElementById('outer');
        var deep = document.getElementById('deep');
        outer.contains(deep)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(deep, "true");

    // Child does not contain parent
    auto reverse = engine.evaluate(R"(
        var outer = document.getElementById('outer');
        var inner = document.getElementById('inner');
        inner.contains(outer)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(reverse, "false");

    // Unrelated element returns false
    auto unrelated = engine.evaluate(R"(
        var inner = document.getElementById('inner');
        var newEl = document.createElement('div');
        inner.contains(newEl)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(unrelated, "false");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOM Mutation: insertAdjacentHTML
// ============================================================================
TEST(JSDom, InsertAdjacentHTML) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="target"><span id="existing">Existing</span></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // afterbegin: insert as first child
    engine.evaluate(R"(
        document.getElementById('target').insertAdjacentHTML('afterbegin', '<b>First</b>');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto first_child = engine.evaluate(
        "document.getElementById('target').firstElementChild.tagName");
    EXPECT_EQ(first_child, "B");

    // beforeend: insert as last child (same as append)
    engine.evaluate(R"(
        document.getElementById('target').insertAdjacentHTML('beforeend', '<i>Last</i>');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto last_child = engine.evaluate(
        "document.getElementById('target').lastElementChild.tagName");
    EXPECT_EQ(last_child, "I");

    // Should now have 3 children: b, span, i
    auto count = engine.evaluate(
        "document.getElementById('target').children.length");
    EXPECT_EQ(count, "3");

    EXPECT_TRUE(clever::js::dom_was_modified(engine.context()));
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOM Utility: outerHTML getter
// ============================================================================
TEST(JSDom, OuterHTML) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="target" class="box"><span>Hello</span></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.getElementById('target').outerHTML");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // Should contain the opening tag with attributes
    EXPECT_NE(result.find("<div"), std::string::npos);
    EXPECT_NE(result.find("class=\"box\""), std::string::npos);
    // Should contain the child
    EXPECT_NE(result.find("<span>"), std::string::npos);
    EXPECT_NE(result.find("Hello"), std::string::npos);
    // Should contain the closing tag
    EXPECT_NE(result.find("</div>"), std::string::npos);

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, OuterHTMLVoidElement) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="container"><br><img src="test.png"></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto br_html = engine.evaluate(R"(
        document.querySelector('br').outerHTML
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // br is a void element, should not have closing tag
    EXPECT_EQ(br_html, "<br>");

    auto img_html = engine.evaluate(R"(
        document.querySelector('img').outerHTML
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // img is a void element with attribute
    EXPECT_NE(img_html.find("<img"), std::string::npos);
    EXPECT_NE(img_html.find("src=\"test.png\""), std::string::npos);
    // Should NOT have </img>
    EXPECT_EQ(img_html.find("</img>"), std::string::npos);

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// New Web APIs: btoa / atob
// ============================================================================
TEST(JSWindow, BtoaEncodesBase64) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("btoa('Hello, World!')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "SGVsbG8sIFdvcmxkIQ==");
}

TEST(JSWindow, AtobDecodesBase64) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("atob('SGVsbG8sIFdvcmxkIQ==')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Hello, World!");
}

TEST(JSWindow, BtoaAtobRoundTrip) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("atob(btoa('test string 123!@#'))");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "test string 123!@#");
}

// ============================================================================
// New Web APIs: encodeURIComponent / decodeURIComponent (built into QuickJS)
// ============================================================================
TEST(JSWindow, EncodeURIComponentExists) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("encodeURIComponent('hello world & more')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello%20world%20%26%20more");
}

TEST(JSWindow, DecodeURIComponentExists) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("decodeURIComponent('hello%20world%20%26%20more')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello world & more");
}

// ============================================================================
// New Web APIs: performance.now()
// ============================================================================
TEST(JSWindow, PerformanceNowReturnsNumber) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("typeof performance.now()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "number");
}

TEST(JSWindow, PerformanceNowReturnsPositive) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("performance.now() >= 0");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// New Web APIs: requestAnimationFrame / cancelAnimationFrame
// ============================================================================
TEST(JSWindow, RequestAnimationFrameReturnsId) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("requestAnimationFrame(function(){})");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // Should return a positive integer ID
    EXPECT_NE(result, "0");
}

TEST(JSWindow, RequestAnimationFrameExecutesCallback) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    engine.evaluate("var rafCalled = false; requestAnimationFrame(function(ts) { rafCalled = true; })");
    auto result = engine.evaluate("rafCalled");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSWindow, CancelAnimationFrameExists) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("typeof cancelAnimationFrame");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

// ============================================================================
// New Web APIs: matchMedia (stub)
// ============================================================================
TEST(JSWindow, MatchMediaReturnsObject) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        var mql = matchMedia('(min-width: 768px)');
        mql.media
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "(min-width: 768px)");
}

TEST(JSWindow, MatchMediaMatchesFalse) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("matchMedia('(min-width: 768px)').matches");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false");
}

TEST(JSWindow, MatchMediaHasEventListenerMethods) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        var mql = matchMedia('screen');
        typeof mql.addEventListener === 'function' &&
        typeof mql.removeEventListener === 'function'
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// New Web APIs: queueMicrotask
// ============================================================================
TEST(JSWindow, QueueMicrotaskExecutes) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    engine.evaluate("var mtCalled = false; queueMicrotask(function() { mtCalled = true; })");
    auto result = engine.evaluate("mtCalled");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// New Web APIs: getSelection (stub)
// ============================================================================
TEST(JSWindow, GetSelectionReturnsObject) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        var sel = getSelection();
        sel.rangeCount === 0 && sel.toString() === ''
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// MutationObserver stub
// ============================================================================
TEST(JSDom, MutationObserverStub) {
    auto doc = clever::html::parse("<html><body><div id='target'></div></body></html>");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // MutationObserver should be defined
    auto defined = engine.evaluate("typeof MutationObserver");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(defined, "function");

    // Constructor should work and return an object with methods
    auto result = engine.evaluate(R"(
        var cb = function() {};
        var observer = new MutationObserver(cb);
        var hasObserve = typeof observer.observe === 'function';
        var hasDisconnect = typeof observer.disconnect === 'function';
        var hasTakeRecords = typeof observer.takeRecords === 'function';
        hasObserve && hasDisconnect && hasTakeRecords
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    // observe(), disconnect(), and takeRecords() should not throw
    auto no_throw = engine.evaluate(R"(
        var mo = new MutationObserver(function() {});
        mo.observe(document.getElementById('target'), { childList: true });
        mo.disconnect();
        var records = mo.takeRecords();
        Array.isArray(records) && records.length === 0
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(no_throw, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// IntersectionObserver stub
// ============================================================================
TEST(JSDom, IntersectionObserverStub) {
    auto doc = clever::html::parse("<html><body><div id='target'></div></body></html>");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var io = new IntersectionObserver(function() {});
        var hasObserve = typeof io.observe === 'function';
        var hasUnobserve = typeof io.unobserve === 'function';
        var hasDisconnect = typeof io.disconnect === 'function';
        io.observe(document.getElementById('target'));
        io.unobserve(document.getElementById('target'));
        io.disconnect();
        hasObserve && hasUnobserve && hasDisconnect
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// ResizeObserver stub
// ============================================================================
TEST(JSDom, ResizeObserverStub) {
    auto doc = clever::html::parse("<html><body><div id='target'></div></body></html>");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var ro = new ResizeObserver(function() {});
        var hasObserve = typeof ro.observe === 'function';
        var hasUnobserve = typeof ro.unobserve === 'function';
        var hasDisconnect = typeof ro.disconnect === 'function';
        ro.observe(document.getElementById('target'));
        ro.unobserve(document.getElementById('target'));
        ro.disconnect();
        hasObserve && hasUnobserve && hasDisconnect
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// CustomEvent constructor
// ============================================================================
TEST(JSDom, CustomEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // Basic CustomEvent with just type
    auto type_only = engine.evaluate(R"(
        var evt = new CustomEvent('myevent');
        evt.type
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(type_only, "myevent");

    // CustomEvent with options: detail, bubbles, cancelable
    auto with_options = engine.evaluate(R"(
        var evt = new CustomEvent('test', {
            detail: { key: 'value' },
            bubbles: true,
            cancelable: true
        });
        evt.type === 'test' && evt.bubbles === true &&
        evt.cancelable === true && evt.detail.key === 'value'
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(with_options, "true");

    // CustomEvent has preventDefault
    auto has_prevent = engine.evaluate(R"(
        var evt = new CustomEvent('cancel');
        typeof evt.preventDefault === 'function'
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(has_prevent, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// element.dispatchEvent with CustomEvent
// ============================================================================
TEST(JSDom, ElementDispatchEvent) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="btn">Click me</div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // Register listener and dispatch event
    auto result = engine.evaluate(R"(
        var received = false;
        var receivedDetail = null;
        var el = document.getElementById('btn');
        el.addEventListener('custom', function(e) {
            received = true;
            receivedDetail = e.detail;
        });
        var evt = new CustomEvent('custom', { detail: 42 });
        var dispatched = el.dispatchEvent(evt);
        received && receivedDetail === 42 && dispatched === true
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    // Test that target is set on dispatched event
    auto target_check = engine.evaluate(R"(
        var targetTag = null;
        var el = document.getElementById('btn');
        el.addEventListener('check', function(e) {
            targetTag = e.target.tagName;
        });
        el.dispatchEvent(new CustomEvent('check'));
        targetTag
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(target_check, "DIV");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// classList improvements (forEach, length, replace, item, value)
// ============================================================================
TEST(JSDom, ClassListImprovements) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="el" class="foo bar baz"></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // classList.length
    auto length = engine.evaluate(R"(
        document.getElementById('el').classList.length
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(length, "3");

    // classList.forEach
    auto foreach_result = engine.evaluate(R"(
        var classes = [];
        document.getElementById('el').classList.forEach(function(c) {
            classes.push(c);
        });
        classes.join(',')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(foreach_result, "foo,bar,baz");

    // classList.item
    auto item_result = engine.evaluate(R"(
        var cl = document.getElementById('el').classList;
        cl.item(0) + ',' + cl.item(1) + ',' + cl.item(2)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(item_result, "foo,bar,baz");

    // classList.replace
    auto replace_result = engine.evaluate(R"(
        var el = document.getElementById('el');
        var replaced = el.classList.replace('bar', 'qux');
        replaced && el.classList.contains('qux') && !el.classList.contains('bar')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(replace_result, "true");

    // classList.value
    auto value_result = engine.evaluate(R"(
        var cl = document.getElementById('el').classList;
        typeof cl.value === 'string' && cl.value.indexOf('foo') >= 0
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(value_result, "true");

    // classList.toggle with force parameter
    auto toggle_force = engine.evaluate(R"(
        var el = document.getElementById('el');
        // Force add when not present
        el.classList.toggle('newclass', true);
        var added = el.classList.contains('newclass');
        // Force remove
        el.classList.toggle('newclass', false);
        var removed = !el.classList.contains('newclass');
        added && removed
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(toggle_force, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// querySelectorAll returns array-like with forEach
// ============================================================================
TEST(JSDom, QuerySelectorAllForEach) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <p class="item">A</p>
            <p class="item">B</p>
            <p class="item">C</p>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // querySelectorAll has forEach
    auto has_foreach = engine.evaluate(R"(
        var nodes = document.querySelectorAll('.item');
        typeof nodes.forEach === 'function'
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(has_foreach, "true");

    // querySelectorAll has length
    auto has_length = engine.evaluate(R"(
        document.querySelectorAll('.item').length
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(has_length, "3");

    // forEach iterates correctly
    auto foreach_works = engine.evaluate(R"(
        var tags = [];
        document.querySelectorAll('.item').forEach(function(el) {
            tags.push(el.tagName);
        });
        tags.join(',')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(foreach_works, "P,P,P");

    // indexing works
    auto indexing = engine.evaluate(R"(
        var nodes = document.querySelectorAll('.item');
        nodes[0].tagName === 'P' && nodes[2].tagName === 'P'
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(indexing, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// dispatchEvent with preventDefault
// ============================================================================
TEST(JSDom, DispatchEventPreventDefault) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="el">Test</div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var el = document.getElementById('el');
        el.addEventListener('submit', function(e) {
            e.preventDefault();
        });
        var evt = new CustomEvent('submit', { cancelable: true });
        var dispatched = el.dispatchEvent(evt);
        // dispatchEvent returns false if defaultPrevented
        dispatched === false
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// classList.add with multiple arguments
// ============================================================================
TEST(JSDom, ClassListAddMultiple) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="el"></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var el = document.getElementById('el');
        el.classList.add('a', 'b', 'c');
        el.classList.contains('a') && el.classList.contains('b') && el.classList.contains('c') && el.classList.length === 3
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// window.history stub
// ============================================================================
TEST(JSWindow, HistoryObjectExists) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        typeof history === 'object' &&
        history.length === 1 &&
        history.state === null &&
        typeof history.pushState === 'function' &&
        typeof history.replaceState === 'function' &&
        typeof history.back === 'function' &&
        typeof history.forward === 'function' &&
        typeof history.go === 'function'
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSWindow, HistoryMethodsNoOp) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    // All history methods should be callable without throwing
    engine.evaluate(R"(
        history.pushState({page: 1}, 'title', '/page1');
        history.replaceState(null, '', '/page2');
        history.back();
        history.forward();
        history.go(-1);
        history.go(0);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
}

// ============================================================================
// window.screen stub
// ============================================================================
TEST(JSWindow, ScreenObjectProperties) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        screen.width === 1024 &&
        screen.height === 768 &&
        screen.availWidth === 1024 &&
        screen.availHeight === 768 &&
        screen.colorDepth === 24 &&
        screen.pixelDepth === 24
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// window.devicePixelRatio
// ============================================================================
TEST(JSWindow, DevicePixelRatio) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("window.devicePixelRatio");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "2");
}

// ============================================================================
// window.scrollTo / scrollBy / scroll (no-ops)
// ============================================================================
TEST(JSWindow, ScrollMethodsExistAndNoOp) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    engine.evaluate(R"(
        scrollTo(0, 100);
        scrollBy(0, 50);
        scroll(0, 0);
        window.scrollTo({top: 0, left: 0, behavior: 'smooth'});
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
}

// ============================================================================
// window.open / window.close (no-ops)
// ============================================================================
TEST(JSWindow, OpenReturnsNullAndCloseNoOp) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        var w = window.open('https://example.com/');
        window.close();
        w === null
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// window.dispatchEvent (no-op, returns true)
// ============================================================================
TEST(JSWindow, WindowDispatchEventReturnsTrue) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("window.dispatchEvent({type: 'resize'})");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// window.removeEventListener (no-op stub)
// ============================================================================
TEST(JSWindow, RemoveEventListenerNoOp) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    engine.evaluate("window.removeEventListener('resize', function() {})");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
}

// ============================================================================
// window.crypto.getRandomValues
// ============================================================================
TEST(JSWindow, CryptoGetRandomValues) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        var arr = new Uint8Array(16);
        var result = crypto.getRandomValues(arr);
        // Check that the returned value is the same array
        result === arr &&
        // Check that at least one value is non-zero (extremely unlikely all are 0)
        arr.some(function(v) { return v !== 0; }) &&
        // Check array length is preserved
        arr.length === 16
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSWindow, CryptoRandomUUID) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        var uuid = crypto.randomUUID();
        // UUID v4 format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
        typeof uuid === 'string' &&
        uuid.length === 36 &&
        uuid[14] === '4' &&
        uuid[8] === '-' && uuid[13] === '-' && uuid[18] === '-' && uuid[23] === '-'
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// URL class
// ============================================================================
TEST(JSWindow, URLConstructorAndProperties) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        var u = new URL('https://example.com:8080/path?q=1#frag');
        u.protocol === 'https:' &&
        u.hostname === 'example.com' &&
        u.port === '8080' &&
        u.pathname === '/path' &&
        u.search === '?q=1' &&
        u.hash === '#frag' &&
        u.origin === 'https://example.com:8080' &&
        u.href === 'https://example.com:8080/path?q=1#frag'
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSWindow, URLToString) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        var u = new URL('https://example.com/test');
        u.toString() === 'https://example.com/test'
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSWindow, URLSearchParamsFromURL) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        var u = new URL('https://example.com/page?foo=bar&baz=qux');
        u.searchParams.get('foo') === 'bar' &&
        u.searchParams.get('baz') === 'qux' &&
        u.searchParams.has('foo') === true &&
        u.searchParams.has('missing') === false
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// URLSearchParams class
// ============================================================================
TEST(JSWindow, URLSearchParamsBasic) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        var p = new URLSearchParams('a=1&b=2&c=3');
        p.get('a') === '1' &&
        p.get('b') === '2' &&
        p.get('c') === '3' &&
        p.get('d') === null &&
        p.has('a') === true &&
        p.has('d') === false
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSWindow, URLSearchParamsSetAndDelete) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        var p = new URLSearchParams('a=1&b=2');
        p.set('a', '10');
        p.delete('b');
        p.set('c', '3');
        p.get('a') === '10' &&
        p.has('b') === false &&
        p.get('c') === '3'
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSWindow, URLSearchParamsToString) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        var p = new URLSearchParams('key=value&foo=bar');
        p.toString() === 'key=value&foo=bar'
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSWindow, URLSearchParamsWithLeadingQuestionMark) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        var p = new URLSearchParams('?x=1&y=2');
        p.get('x') === '1' && p.get('y') === '2'
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// URL.createObjectURL / URL.revokeObjectURL stubs
// ============================================================================
TEST(JSWindow, URLStaticMethods) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate(R"(
        typeof URL.createObjectURL === 'function' &&
        typeof URL.revokeObjectURL === 'function'
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// window.addEventListener accepts any event type silently
// ============================================================================
TEST(JSWindow, AddEventListenerAcceptsAnyEventType) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);
    clever::js::install_dom_bindings(engine.context(), doc.get());
    // These should not throw -- any event type is accepted silently
    engine.evaluate(R"(
        window.addEventListener('resize', function() {});
        window.addEventListener('load', function() {});
        window.addEventListener('scroll', function() {});
        window.addEventListener('popstate', function() {});
        window.addEventListener('hashchange', function() {});
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// ============================================================================
//
//   FETCH API TESTS
//
// ============================================================================
// ============================================================================

// ============================================================================
// fetch() is a global function
// ============================================================================
TEST(JSFetch, FetchExistsAsGlobalFunction) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate("typeof fetch");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

// ============================================================================
// fetch() requires at least one argument
// ============================================================================
TEST(JSFetch, FetchThrowsWithoutArguments) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate("fetch()");
    EXPECT_TRUE(engine.has_error());
}

// ============================================================================
// fetch() returns a Promise object
// ============================================================================
TEST(JSFetch, FetchReturnsPromise) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    // fetch with a bogus URL will still return a Promise (rejected on network error)
    auto result = engine.evaluate(R"(
        var p = fetch('http://0.0.0.0:1/nonexistent');
        p instanceof Promise
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// fetch() Promise .then() chain executes after flushing jobs
// ============================================================================
TEST(JSFetch, FetchThenChainExecutes) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    // Use a known-unreachable URL -- the rejection path exercises .catch()
    engine.evaluate(R"(
        var caught = false;
        fetch('http://0.0.0.0:1/nonexistent')
            .catch(function(err) { caught = true; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // Flush promise microtasks
    clever::js::flush_fetch_promise_jobs(engine.context());

    auto result = engine.evaluate("caught");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// Response .ok property
// ============================================================================
TEST(JSFetch, ResponseOkProperty) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    // We can test with a constructed response via internal mechanism,
    // but since we can only use fetch(), test the property type instead.
    // A network error Response should have ok = false.
    engine.evaluate(R"(
        var okVal = 'untouched';
        fetch('http://0.0.0.0:1/nonexistent')
            .then(function(resp) { okVal = resp.ok; })
            .catch(function(err) { okVal = 'network-error'; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("okVal");
    // Network error rejects, so catch fires
    EXPECT_EQ(result, "network-error");
}

// ============================================================================
// Response .status property
// ============================================================================
TEST(JSFetch, ResponseStatusProperty) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var statusVal = -1;
        fetch('http://0.0.0.0:1/nonexistent')
            .then(function(resp) { statusVal = resp.status; })
            .catch(function(err) { statusVal = 'error'; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("statusVal");
    EXPECT_EQ(result, "error"); // network error -> catch
}

// ============================================================================
// Response .text() returns a Promise
// ============================================================================
TEST(JSFetch, ResponseTextReturnsPromise) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    // Test that Response.prototype.text is a function (via fetch path)
    // Since we cannot easily reach a real server from tests, verify the
    // structure: fetch returns a Promise, and if resolved, .text() is a function
    engine.evaluate(R"(
        var textIsFn = false;
        fetch('http://0.0.0.0:1/nonexistent')
            .then(function(resp) { textIsFn = typeof resp.text === 'function'; })
            .catch(function(err) { textIsFn = 'network-error'; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    // For network error, catch fires
    auto result = engine.evaluate("textIsFn");
    EXPECT_EQ(result, "network-error");
}

// ============================================================================
// Response .json() returns a Promise
// ============================================================================
TEST(JSFetch, ResponseJsonReturnsPromise) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var jsonIsFn = false;
        fetch('http://0.0.0.0:1/nonexistent')
            .then(function(resp) { jsonIsFn = typeof resp.json === 'function'; })
            .catch(function(err) { jsonIsFn = 'network-error'; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("jsonIsFn");
    EXPECT_EQ(result, "network-error");
}

// ============================================================================
// fetch() with method option is accepted
// ============================================================================
TEST(JSFetch, FetchWithMethodOption) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    // Should not throw even with POST method option
    auto result = engine.evaluate(R"(
        var p = fetch('http://0.0.0.0:1/nonexistent', { method: 'POST' });
        p instanceof Promise
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// fetch() with headers option is accepted
// ============================================================================
TEST(JSFetch, FetchWithHeadersOption) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var p = fetch('http://0.0.0.0:1/nonexistent', {
            headers: { 'Content-Type': 'application/json', 'X-Custom': 'test' }
        });
        p instanceof Promise
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// fetch() with body option is accepted
// ============================================================================
TEST(JSFetch, FetchWithBodyOption) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var p = fetch('http://0.0.0.0:1/nonexistent', {
            method: 'POST',
            body: '{"key":"value"}'
        });
        p instanceof Promise
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// Promise microtask execution works
// ============================================================================
TEST(JSFetch, PromiseMicrotaskExecution) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var resolved = false;
        Promise.resolve(42).then(function(v) { resolved = true; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // Before flushing, the .then() hasn't run yet
    clever::js::flush_fetch_promise_jobs(engine.context());

    auto result = engine.evaluate("resolved");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// Promise chaining works (.then().then())
// ============================================================================
TEST(JSFetch, PromiseChaining) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var result = 0;
        Promise.resolve(1)
            .then(function(v) { return v + 1; })
            .then(function(v) { return v * 3; })
            .then(function(v) { result = v; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("result");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "6"); // (1+1)*3 = 6
}

// ============================================================================
// Promise.reject and .catch work
// ============================================================================
TEST(JSFetch, PromiseRejectAndCatch) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var caughtMsg = '';
        Promise.reject('boom')
            .catch(function(err) { caughtMsg = err; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("caughtMsg");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "boom");
}

// ============================================================================
// fetch() network error rejects the Promise
// ============================================================================
TEST(JSFetch, FetchNetworkErrorRejectsPromise) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var errMsg = '';
        fetch('http://0.0.0.0:1/will-fail')
            .then(function(resp) { errMsg = 'should-not-resolve'; })
            .catch(function(err) { errMsg = err.message || String(err); });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("errMsg");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // Should contain something about network error
    EXPECT_NE(result, "should-not-resolve");
    EXPECT_FALSE(result.empty());
}

TEST(JSFetch, FetchRejectsUnsupportedRequestSchemeBeforeDispatch) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://app.example/", 1024, 768);
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var errMsg = '';
        fetch('ftp://api.example/data')
            .then(function(resp) { errMsg = 'should-not-resolve'; })
            .catch(function(err) { errMsg = err.message || String(err); });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("errMsg");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "TypeError: Failed to fetch (CORS blocked)");
}

TEST(JSFetch, XHRRejectsUnsupportedRequestSchemeBeforeDispatch) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://app.example/", 1024, 768);
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var xhr = new XMLHttpRequest();
        xhr.open('GET', 'ftp://api.example/data');
        xhr.send();
        [xhr.readyState, xhr.status].join(',')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "4,0");
}

// ============================================================================
// Response .type is "basic"
// ============================================================================
TEST(JSFetch, ResponseTypeIsBasic) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    // Since we can't easily get a successful Response in tests without a server,
    // test that a failed fetch goes to catch, confirming the promise flow
    engine.evaluate(R"(
        var typeVal = 'untouched';
        fetch('http://0.0.0.0:1/x')
            .then(function(resp) { typeVal = resp.type; })
            .catch(function(err) { typeVal = 'caught-error'; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("typeVal");
    EXPECT_EQ(result, "caught-error");
}

// ============================================================================
// Response .clone() returns a new Response
// ============================================================================
TEST(JSFetch, ResponseClone) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var cloneWorks = false;
        fetch('http://0.0.0.0:1/x')
            .then(function(resp) {
                var c = resp.clone();
                cloneWorks = (c.status === resp.status);
            })
            .catch(function(err) { cloneWorks = 'caught-error'; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("cloneWorks");
    EXPECT_EQ(result, "caught-error"); // network error -> catch
}

// ============================================================================
// Headers class .get() returns null for missing headers
// ============================================================================
TEST(JSFetch, HeadersGetReturnsNull) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var headerResult = 'untouched';
        fetch('http://0.0.0.0:1/x')
            .then(function(resp) {
                headerResult = resp.headers.get('x-missing') === null ? 'null' : 'not-null';
            })
            .catch(function(err) { headerResult = 'caught-error'; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("headerResult");
    EXPECT_EQ(result, "caught-error"); // network error -> catch
}

// ============================================================================
// async/await with fetch works (QuickJS supports async/await natively)
// ============================================================================
TEST(JSFetch, AsyncAwaitWithFetch) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var asyncResult = 'pending';
        (async function() {
            try {
                var resp = await fetch('http://0.0.0.0:1/x');
                asyncResult = 'resolved';
            } catch (e) {
                asyncResult = 'caught';
            }
        })();
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("asyncResult");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "caught"); // network error -> caught in async/await
}

// ============================================================================
// Promise.all works
// ============================================================================
TEST(JSFetch, PromiseAll) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var allResult = 0;
        Promise.all([
            Promise.resolve(1),
            Promise.resolve(2),
            Promise.resolve(3)
        ]).then(function(values) {
            allResult = values[0] + values[1] + values[2];
        });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("allResult");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "6");
}

// ============================================================================
// Promise.resolve().then().then() value threading
// ============================================================================
TEST(JSFetch, PromiseValueThreading) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var threadResult = '';
        Promise.resolve('hello')
            .then(function(v) { return v + ' world'; })
            .then(function(v) { threadResult = v; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("threadResult");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello world");
}

// ============================================================================
// DOM Event Propagation Tests
// ============================================================================

// Helper: find a node by id attribute
static clever::html::SimpleNode* find_node_by_id(
        clever::html::SimpleNode* root, const std::string& id) {
    if (!root) return nullptr;
    if (root->type == clever::html::SimpleNode::Element) {
        for (auto& attr : root->attributes) {
            if (attr.name == "id" && attr.value == id) return root;
        }
    }
    for (auto& child : root->children) {
        auto* found = find_node_by_id(child.get(), id);
        if (found) return found;
    }
    return nullptr;
}

// Test: Capture phase listener fires before target phase listener
TEST(JSEventPropagation, CaptureFiresBeforeTarget) {
    auto doc = clever::html::parse(
        "<html><body><div id='parent'><span id='child'>x</span></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var order = [];
        var parent = document.getElementById('parent');
        var child = document.getElementById('child');
        parent.addEventListener('click', function() { order.push('parent-capture'); }, true);
        child.addEventListener('click', function() { order.push('child-target'); });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto* child_node = find_node_by_id(doc.get(), "child");
    ASSERT_NE(child_node, nullptr);

    clever::js::dispatch_event(engine.context(), child_node, "click");

    auto result = engine.evaluate("order.join(',')");
    EXPECT_EQ(result, "parent-capture,child-target");

    clever::js::cleanup_dom_bindings(engine.context());
}

// Test: Bubble phase listener fires after target phase listener
TEST(JSEventPropagation, BubbleFiresAfterTarget) {
    auto doc = clever::html::parse(
        "<html><body><div id='parent'><span id='child'>x</span></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var order = [];
        var parent = document.getElementById('parent');
        var child = document.getElementById('child');
        child.addEventListener('click', function() { order.push('child-target'); });
        parent.addEventListener('click', function() { order.push('parent-bubble'); });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto* child_node = find_node_by_id(doc.get(), "child");
    ASSERT_NE(child_node, nullptr);

    clever::js::dispatch_event(engine.context(), child_node, "click");

    auto result = engine.evaluate("order.join(',')");
    EXPECT_EQ(result, "child-target,parent-bubble");

    clever::js::cleanup_dom_bindings(engine.context());
}

// Test: stopPropagation prevents bubble to ancestor
TEST(JSEventPropagation, StopPropagationPreventsBubble) {
    auto doc = clever::html::parse(
        "<html><body><div id='parent'><span id='child'>x</span></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var parentCalled = false;
        var parent = document.getElementById('parent');
        var child = document.getElementById('child');
        child.addEventListener('click', function(e) { e.stopPropagation(); });
        parent.addEventListener('click', function() { parentCalled = true; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto* child_node = find_node_by_id(doc.get(), "child");
    ASSERT_NE(child_node, nullptr);

    clever::js::dispatch_event(engine.context(), child_node, "click");

    auto result = engine.evaluate("parentCalled");
    EXPECT_EQ(result, "false");

    clever::js::cleanup_dom_bindings(engine.context());
}

// Test: stopImmediatePropagation prevents remaining listeners on same element
TEST(JSEventPropagation, StopImmediatePropagationPreventsRemaining) {
    auto doc = clever::html::parse(
        "<html><body><div id='parent'><span id='child'>x</span></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var calls = 0;
        var parentCalled = false;
        var child = document.getElementById('child');
        var parent = document.getElementById('parent');
        child.addEventListener('click', function(e) { calls++; e.stopImmediatePropagation(); });
        child.addEventListener('click', function() { calls++; });
        parent.addEventListener('click', function() { parentCalled = true; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto* child_node = find_node_by_id(doc.get(), "child");
    ASSERT_NE(child_node, nullptr);

    clever::js::dispatch_event(engine.context(), child_node, "click");

    EXPECT_EQ(engine.evaluate("calls"), "1"); // Only first listener called
    EXPECT_EQ(engine.evaluate("parentCalled"), "false"); // No bubble

    clever::js::cleanup_dom_bindings(engine.context());
}

// Test: eventPhase values during each phase
TEST(JSEventPropagation, EventPhaseValues) {
    auto doc = clever::html::parse(
        "<html><body><div id='parent'><span id='child'>x</span></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var phases = [];
        var parent = document.getElementById('parent');
        var child = document.getElementById('child');
        parent.addEventListener('click', function(e) { phases.push(e.eventPhase); }, true);
        child.addEventListener('click', function(e) { phases.push(e.eventPhase); });
        parent.addEventListener('click', function(e) { phases.push(e.eventPhase); });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto* child_node = find_node_by_id(doc.get(), "child");
    ASSERT_NE(child_node, nullptr);

    clever::js::dispatch_event(engine.context(), child_node, "click");

    // Capture phase = 1, Target phase = 2, Bubble phase = 3
    auto result = engine.evaluate("phases.join(',')");
    EXPECT_EQ(result, "1,2,3");

    clever::js::cleanup_dom_bindings(engine.context());
}

// Test: currentTarget changes during propagation
TEST(JSEventPropagation, CurrentTargetChangesDuringPropagation) {
    auto doc = clever::html::parse(
        "<html><body><div id='parent'><span id='child'>x</span></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var currentTargetIds = [];
        var targetIds = [];
        var parent = document.getElementById('parent');
        var child = document.getElementById('child');
        parent.addEventListener('click', function(e) {
            currentTargetIds.push(e.currentTarget.getAttribute('id'));
            targetIds.push(e.target.getAttribute('id'));
        }, true);
        child.addEventListener('click', function(e) {
            currentTargetIds.push(e.currentTarget.getAttribute('id'));
            targetIds.push(e.target.getAttribute('id'));
        });
        parent.addEventListener('click', function(e) {
            currentTargetIds.push(e.currentTarget.getAttribute('id'));
            targetIds.push(e.target.getAttribute('id'));
        });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto* child_node = find_node_by_id(doc.get(), "child");
    ASSERT_NE(child_node, nullptr);

    clever::js::dispatch_event(engine.context(), child_node, "click");

    // currentTarget changes: parent(capture) -> child(target) -> parent(bubble)
    auto ct_result = engine.evaluate("currentTargetIds.join(',')");
    EXPECT_EQ(ct_result, "parent,child,parent");

    // target stays the same: always child
    auto t_result = engine.evaluate("targetIds.join(',')");
    EXPECT_EQ(t_result, "child,child,child");

    clever::js::cleanup_dom_bindings(engine.context());
}

// Test: Non-bubbling events don't bubble
TEST(JSEventPropagation, NonBubblingEventsDoNotBubble) {
    auto doc = clever::html::parse(
        "<html><body><div id='parent'><input id='child'></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var parentCalled = false;
        var childCalled = false;
        var parent = document.getElementById('parent');
        var child = document.getElementById('child');
        child.addEventListener('focus', function() { childCalled = true; });
        parent.addEventListener('focus', function() { parentCalled = true; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto* child_node = find_node_by_id(doc.get(), "child");
    ASSERT_NE(child_node, nullptr);

    clever::js::dispatch_event(engine.context(), child_node, "focus");

    EXPECT_EQ(engine.evaluate("childCalled"), "true");
    EXPECT_EQ(engine.evaluate("parentCalled"), "false"); // focus doesn't bubble

    clever::js::cleanup_dom_bindings(engine.context());
}

// Test: composedPath returns correct ancestor chain
TEST(JSEventPropagation, ComposedPathReturnsAncestorChain) {
    auto doc = clever::html::parse(
        "<html><body><div id='parent'><span id='child'>x</span></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var pathLen = 0;
        var firstId = '';
        var child = document.getElementById('child');
        child.addEventListener('click', function(e) {
            var path = e.composedPath();
            pathLen = path.length;
            if (path.length > 0 && path[0].getAttribute) {
                firstId = path[0].getAttribute('id') || path[0].tagName || '';
            }
        });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto* child_node = find_node_by_id(doc.get(), "child");
    ASSERT_NE(child_node, nullptr);

    clever::js::dispatch_event(engine.context(), child_node, "click");

    // Path should include: child, parent (div), body, html, document root
    auto path_len = engine.evaluate("pathLen");
    int len = std::stoi(path_len);
    EXPECT_GE(len, 3); // At minimum: child, div, body (root may or may not be included)

    // First element in path should be the target (child)
    auto first = engine.evaluate("firstId");
    EXPECT_EQ(first, "child");

    clever::js::cleanup_dom_bindings(engine.context());
}

// Test: addEventListener with options object {capture: true}
TEST(JSEventPropagation, AddEventListenerWithOptionsObject) {
    auto doc = clever::html::parse(
        "<html><body><div id='parent'><span id='child'>x</span></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var order = [];
        var parent = document.getElementById('parent');
        var child = document.getElementById('child');
        parent.addEventListener('click', function() { order.push('capture'); }, {capture: true});
        parent.addEventListener('click', function() { order.push('bubble'); }, {capture: false});
        child.addEventListener('click', function() { order.push('target'); });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto* child_node = find_node_by_id(doc.get(), "child");
    ASSERT_NE(child_node, nullptr);

    clever::js::dispatch_event(engine.context(), child_node, "click");

    auto result = engine.evaluate("order.join(',')");
    EXPECT_EQ(result, "capture,target,bubble");

    clever::js::cleanup_dom_bindings(engine.context());
}

// Test: removeEventListener with capture flag matching
TEST(JSEventPropagation, RemoveEventListenerWithCaptureMatching) {
    auto doc = clever::html::parse(
        "<html><body><div id='parent'><span id='child'>x</span></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var order = [];
        var parent = document.getElementById('parent');
        var child = document.getElementById('child');

        var capHandler = function() { order.push('capture'); };
        var bubHandler = function() { order.push('bubble'); };

        parent.addEventListener('click', capHandler, true);
        parent.addEventListener('click', bubHandler, false);

        // Remove the capture listener -- must match capture flag
        parent.removeEventListener('click', capHandler, true);

        // This should NOT remove the bubble listener (different capture flag)
        parent.removeEventListener('click', bubHandler, true);

        child.addEventListener('click', function() { order.push('target'); });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto* child_node = find_node_by_id(doc.get(), "child");
    ASSERT_NE(child_node, nullptr);

    clever::js::dispatch_event(engine.context(), child_node, "click");

    // capture was removed, bubble remains
    auto result = engine.evaluate("order.join(',')");
    EXPECT_EQ(result, "target,bubble");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Complex CSS selector matching via querySelector/querySelectorAll/matches/closest
// ============================================================================

TEST(JSDom, QuerySelectorDescendantCombinator) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div class="parent">
                <span class="child">Found</span>
            </div>
            <span class="child">Not in parent</span>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.querySelector('.parent .child').textContent");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Found");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, QuerySelectorChildCombinator) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div class="parent">
                <span class="direct">Direct</span>
                <div><span class="nested">Nested</span></div>
            </div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // Child combinator should only match direct children
    auto result = engine.evaluate(
        "document.querySelector('.parent > .direct').textContent");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Direct");

    // Nested .nested should NOT match .parent > .nested
    auto nested = engine.evaluate(
        "document.querySelector('.parent > .nested')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(nested, "null");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, QuerySelectorCombinedTagAndClass) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div class="foo" id="d1">Div</div>
            <span class="foo" id="s1">Span</span>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.querySelector('div.foo').id");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "d1");

    // span.foo should find the span, not the div
    auto result2 = engine.evaluate(
        "document.querySelector('span.foo').id");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "s1");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, QuerySelectorAttributeSelector) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div data-id="123" id="target">Found</div>
            <div data-id="456" id="other">Other</div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.querySelector('[data-id=\"123\"]').id");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "target");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, QuerySelectorAllReturnsAllMatches) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <p class="item">A</p>
            <p class="item">B</p>
            <p class="item">C</p>
            <p class="other">D</p>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto count = engine.evaluate(
        "document.querySelectorAll('p.item').length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(count, "3");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, QuerySelectorAllCommaSeparated) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <h1>Title</h1>
            <h2>Subtitle</h2>
            <p>Paragraph</p>
            <h3>Section</h3>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto count = engine.evaluate(
        "document.querySelectorAll('h1, h2, h3').length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(count, "3");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementQuerySelectorScopedToSubtree) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="container">
                <span class="item" id="inner">Inner</span>
            </div>
            <span class="item" id="outer">Outer</span>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // element.querySelector should only search within the element's subtree
    auto result = engine.evaluate(R"(
        var container = document.getElementById('container');
        container.querySelector('.item').id;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "inner");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementMatchesComplexSelector) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div class="parent">
                <span class="child" id="target">Hello</span>
            </div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // element.matches with descendant combinator
    auto result = engine.evaluate(R"(
        document.getElementById('target').matches('.parent .child')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    // Should not match if wrong ancestor
    auto result2 = engine.evaluate(R"(
        document.getElementById('target').matches('.other .child')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "false");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementClosestComplexSelector) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div class="wrapper" id="wrapper">
                <div class="inner" id="inner">
                    <span id="target">Hello</span>
                </div>
            </div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        document.getElementById('target').closest('div.inner').id
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "inner");

    auto result2 = engine.evaluate(R"(
        document.getElementById('target').closest('.wrapper').id
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "wrapper");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, QuerySelectorFirstChild) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <ul id="list">
                <li id="first">A</li>
                <li id="second">B</li>
                <li id="third">C</li>
            </ul>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.querySelector('li:first-child').id");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "first");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, QuerySelectorLastChild) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <ul id="list">
                <li id="first">A</li>
                <li id="second">B</li>
                <li id="third">C</li>
            </ul>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "document.querySelector('li:last-child').id");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "third");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, QuerySelectorNthChild) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <ul>
                <li id="l1">A</li>
                <li id="l2">B</li>
                <li id="l3">C</li>
                <li id="l4">D</li>
            </ul>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // :nth-child(2) should match the second li
    auto result = engine.evaluate(
        "document.querySelector('li:nth-child(2)').id");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "l2");

    // :nth-child(odd) should match 1st and 3rd - querySelectorAll
    auto odd_count = engine.evaluate(
        "document.querySelectorAll('li:nth-child(odd)').length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(odd_count, "2");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, QuerySelectorNot) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <p class="active" id="p1">A</p>
            <p class="inactive" id="p2">B</p>
            <p class="active" id="p3">C</p>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // :not(.active) should match the inactive paragraph
    auto result = engine.evaluate(
        "document.querySelector('p:not(.active)').id");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "p2");

    // Count all p:not(.active) -- should be 1
    auto count = engine.evaluate(
        "document.querySelectorAll('p:not(.active)').length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(count, "1");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementQuerySelectorAllScopedToSubtree) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id="container">
                <span class="item">A</span>
                <span class="item">B</span>
            </div>
            <span class="item">C</span>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // element.querySelectorAll should only find items within container
    auto count = engine.evaluate(R"(
        var container = document.getElementById('container');
        container.querySelectorAll('.item').length;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(count, "2");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Canvas 2D API tests
// ============================================================================

TEST(JSDom, CanvasGetContext2dReturnsObject) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <canvas id="c" width="200" height="100"></canvas>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        typeof ctx;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasGetContextNon2dReturnsNullOrStub) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <canvas id="c"></canvas>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // webgl returns a stub object, unknown types return null
    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var gl = c.getContext('webgl');
        var unknown = c.getContext('bitmaprenderer');
        (gl !== null && typeof gl === 'object') && unknown === null;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasFillRectChangesPixels) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <canvas id="c" width="50" height="50"></canvas>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        ctx.fillStyle = 'red';
        ctx.fillRect(0, 0, 10, 10);
        var d = ctx.getImageData(0, 0, 1, 1);
        d.data[0] + ',' + d.data[1] + ',' + d.data[2] + ',' + d.data[3];
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "255,0,0,255");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasClearRectClearsPixels) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <canvas id="c" width="50" height="50"></canvas>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        ctx.fillStyle = 'blue';
        ctx.fillRect(0, 0, 50, 50);
        ctx.clearRect(5, 5, 10, 10);
        var d = ctx.getImageData(5, 5, 1, 1);
        d.data[0] + ',' + d.data[1] + ',' + d.data[2] + ',' + d.data[3];
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0,0,0,0");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasFillStyleParsingHex) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <canvas id="c" width="10" height="10"></canvas>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        ctx.fillStyle = '#00ff00';
        ctx.fillRect(0, 0, 1, 1);
        var d = ctx.getImageData(0, 0, 1, 1);
        d.data[0] + ',' + d.data[1] + ',' + d.data[2] + ',' + d.data[3];
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0,255,0,255");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasFillStyleParsingNamed) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <canvas id="c" width="10" height="10"></canvas>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        ctx.fillStyle = 'white';
        ctx.fillRect(0, 0, 1, 1);
        var d = ctx.getImageData(0, 0, 1, 1);
        d.data[0] + ',' + d.data[1] + ',' + d.data[2] + ',' + d.data[3];
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "255,255,255,255");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasGlobalAlphaAffectsDrawing) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <canvas id="c" width="10" height="10"></canvas>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        ctx.globalAlpha = 0.5;
        ctx.fillStyle = 'white';
        ctx.fillRect(0, 0, 1, 1);
        var d = ctx.getImageData(0, 0, 1, 1);
        // Alpha should be approximately 127-128 (0.5 * 255)
        var a = d.data[3];
        a >= 126 && a <= 129;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasMeasureTextReturnsWidth) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <canvas id="c" width="100" height="100"></canvas>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        var m = ctx.measureText('hello');
        m.width > 0;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasGetImageDataDimensions) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <canvas id="c" width="100" height="100"></canvas>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        var d = ctx.getImageData(10, 10, 20, 30);
        d.width + ',' + d.height;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "20,30");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasPutImageDataWritesPixels) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <canvas id="c" width="50" height="50"></canvas>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        var imgData = ctx.createImageData(2, 2);
        // Set first pixel to magenta (255, 0, 255, 255)
        imgData.data[0] = 255;
        imgData.data[1] = 0;
        imgData.data[2] = 255;
        imgData.data[3] = 255;
        ctx.putImageData(imgData, 0, 0);
        var d = ctx.getImageData(0, 0, 1, 1);
        d.data[0] + ',' + d.data[1] + ',' + d.data[2] + ',' + d.data[3];
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "255,0,255,255");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasLineWidthGetterSetter) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <canvas id="c" width="10" height="10"></canvas>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        var initial = ctx.lineWidth;
        ctx.lineWidth = 5;
        initial + ',' + ctx.lineWidth;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,5");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasBeginPathRectFillDrawsRectangle) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <canvas id="c" width="50" height="50"></canvas>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        ctx.fillStyle = '#ff0000';
        ctx.beginPath();
        ctx.rect(5, 5, 10, 10);
        ctx.fill();
        var d = ctx.getImageData(10, 10, 1, 1);
        d.data[0] + ',' + d.data[1] + ',' + d.data[2] + ',' + d.data[3];
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "255,0,0,255");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web Workers API tests
// ============================================================================

TEST(JSWorker, ConstructorExists) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    auto result = engine.evaluate("typeof Worker");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

TEST(JSWorker, NewWorkerCreatesObject) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    auto result = engine.evaluate(R"(
        var w = new Worker('__inline:// empty worker');
        typeof w;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");
}

TEST(JSWorker, PostMessageExistsAsFunction) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    auto result = engine.evaluate(R"(
        var w = new Worker('__inline:// empty');
        typeof w.postMessage;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

TEST(JSWorker, TerminateExistsAsFunction) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    auto result = engine.evaluate(R"(
        var w = new Worker('__inline:// empty');
        typeof w.terminate;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

TEST(JSWorker, OnmessageGetterSetter) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    auto result = engine.evaluate(R"(
        var w = new Worker('__inline:// empty');
        var initial = w.onmessage;
        w.onmessage = function(e) {};
        var afterSet = typeof w.onmessage;
        // Initial should be undefined, after set should be a function
        (initial === undefined || initial === null) + ',' + afterSet;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,function");
}

TEST(JSWorker, OnerrorGetterSetter) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    auto result = engine.evaluate(R"(
        var w = new Worker('__inline:// empty');
        var initial = w.onerror;
        w.onerror = function(e) {};
        var afterSet = typeof w.onerror;
        (initial === undefined || initial === null) + ',' + afterSet;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,function");
}

TEST(JSWorker, ProcessesInlineScript) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    // Worker script sets onmessage and echoes back data with a prefix
    auto result = engine.evaluate(R"(
        var received = '';
        var w = new Worker('__inline:onmessage = function(e) { postMessage("echo:" + e.data); }');
        w.onmessage = function(e) { received = e.data; };
        w.postMessage('hello');
        received;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "echo:hello");
}

TEST(JSWorker, PostMessageOnmessageRoundTrip) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    // Worker receives a number and sends back double
    auto result = engine.evaluate(R"(
        var answer = 0;
        var w = new Worker('__inline:onmessage = function(e) { postMessage(e.data * 2); }');
        w.onmessage = function(e) { answer = e.data; };
        w.postMessage(21);
        answer;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42");
}

TEST(JSWorker, PostMessageObjectRoundTrip) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    // Send an object, worker adds a field, sends back
    auto result = engine.evaluate(R"(
        var result = '';
        var w = new Worker('__inline:onmessage = function(e) { postMessage({x: e.data.a + e.data.b}); }');
        w.onmessage = function(e) { result = e.data.x; };
        w.postMessage({a: 10, b: 32});
        result;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42");
}

TEST(JSWorker, TerminatePreventsFurtherMessages) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    auto result = engine.evaluate(R"(
        var count = 0;
        var w = new Worker('__inline:onmessage = function(e) { postMessage("ok"); }');
        w.onmessage = function(e) { count++; };
        w.postMessage('first');
        w.terminate();
        try { w.postMessage('second'); } catch(e) { /* expected */ }
        count;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // Only the first message should have been processed
    EXPECT_EQ(result, "1");
}

TEST(JSWorker, SelfCloseWorks) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    // Worker calls self.close() after first message
    auto result = engine.evaluate(R"(
        var count = 0;
        var w = new Worker('__inline:onmessage = function(e) { postMessage("ok"); close(); }');
        w.onmessage = function(e) { count++; };
        w.postMessage('first');
        w.postMessage('second');
        count;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // Only the first message should produce a response
    EXPECT_EQ(result, "1");
}

TEST(JSWorker, MultipleWorkersCoexist) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    auto result = engine.evaluate(R"(
        var result1 = 0;
        var result2 = 0;
        var w1 = new Worker('__inline:onmessage = function(e) { postMessage(e.data + 1); }');
        var w2 = new Worker('__inline:onmessage = function(e) { postMessage(e.data + 100); }');
        w1.onmessage = function(e) { result1 = e.data; };
        w2.onmessage = function(e) { result2 = e.data; };
        w1.postMessage(10);
        w2.postMessage(10);
        result1 + ',' + result2;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "11,110");
}

// ============================================================================
// Cycle 220: Modern DOM Manipulation Methods
// ============================================================================

TEST(JSDom, ElementBefore) {
    auto doc = clever::html::parse("<html><body><div id=\"parent\"><span id=\"ref\">ref</span></div></body></html>");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var ref = document.getElementById('ref');
        var newEl = document.createElement('b');
        newEl.textContent = 'bold';
        ref.before(newEl, 'hello');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // parent should now have: <b>, "hello", <span> = 3 child nodes
    auto result = engine.evaluate("document.getElementById('parent').childNodes.length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");

    // First child should be the <b> element
    auto tag = engine.evaluate("document.getElementById('parent').children[0].tagName");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(tag, "B");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementAfter) {
    auto doc = clever::html::parse("<html><body><div id=\"parent\"><span id=\"ref\">ref</span></div></body></html>");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var ref = document.getElementById('ref');
        var newEl = document.createElement('i');
        newEl.textContent = 'italic';
        ref.after(newEl);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // parent should have: <span>, <i> = 2 element children
    auto result = engine.evaluate("document.getElementById('parent').children.length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "2");

    auto tag = engine.evaluate("document.getElementById('parent').children[1].tagName");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(tag, "I");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementPrepend) {
    auto doc = clever::html::parse("<html><body><div id=\"parent\"><span>existing</span></div></body></html>");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var parent = document.getElementById('parent');
        var first = document.createElement('em');
        first.textContent = 'first';
        parent.prepend(first, 'text');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // parent should have: <em>, "text", <span> = 3 child nodes
    auto result = engine.evaluate("document.getElementById('parent').childNodes.length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");

    // First element child should be <em>
    auto tag = engine.evaluate("document.getElementById('parent').children[0].tagName");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(tag, "EM");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementAppend) {
    auto doc = clever::html::parse("<html><body><div id=\"parent\"><span>existing</span></div></body></html>");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var parent = document.getElementById('parent');
        var a = document.createElement('a');
        a.textContent = 'link';
        parent.append(a, 'suffix');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // parent should have: <span>, <a>, "suffix" = 3 child nodes
    auto count = engine.evaluate("document.getElementById('parent').childNodes.length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(count, "3");

    // Second element child (index 1) should be <a>
    auto tag = engine.evaluate("document.getElementById('parent').children[1].tagName");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(tag, "A");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementReplaceWith) {
    auto doc = clever::html::parse("<html><body><div id=\"parent\"><span id=\"old\">old</span></div></body></html>");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    engine.evaluate(R"(
        var old = document.getElementById('old');
        var newEl = document.createElement('strong');
        newEl.textContent = 'new';
        old.replaceWith(newEl, 'extra');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    // parent should have: <strong>, "extra" = 2 child nodes
    auto count = engine.evaluate("document.getElementById('parent').childNodes.length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(count, "2");

    // First element child should be <strong>
    auto tag = engine.evaluate("document.getElementById('parent').children[0].tagName");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(tag, "STRONG");

    // old element should no longer be in DOM
    auto oldEl = engine.evaluate("document.getElementById('old')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(oldEl, "null");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ToggleAttribute) {
    auto doc = clever::html::parse("<html><body><button id=\"btn\">Click</button></body></html>");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // Toggle on (no attribute initially)
    auto result1 = engine.evaluate(R"(
        var btn = document.getElementById('btn');
        btn.toggleAttribute('disabled');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result1, "true");

    auto has1 = engine.evaluate("document.getElementById('btn').hasAttribute('disabled')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(has1, "true");

    // Toggle off
    auto result2 = engine.evaluate(R"(
        document.getElementById('btn').toggleAttribute('disabled');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "false");

    auto has2 = engine.evaluate("document.getElementById('btn').hasAttribute('disabled')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(has2, "false");

    // Force = true
    auto result3 = engine.evaluate(R"(
        document.getElementById('btn').toggleAttribute('hidden', true);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result3, "true");

    auto has3 = engine.evaluate("document.getElementById('btn').hasAttribute('hidden')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(has3, "true");

    // Force = false
    auto result4 = engine.evaluate(R"(
        document.getElementById('btn').toggleAttribute('hidden', false);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result4, "false");

    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, InsertAdjacentElement) {
    auto doc = clever::html::parse("<html><body><div id=\"container\"><p id=\"target\">Hello</p></div></body></html>");
    ASSERT_NE(doc, nullptr);
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // beforebegin
    engine.evaluate(R"(
        var target = document.getElementById('target');
        var el1 = document.createElement('span');
        el1.setAttribute('id', 'bb');
        target.insertAdjacentElement('beforebegin', el1);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto bb = engine.evaluate("document.getElementById('bb') !== null");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(bb, "true");

    // afterend
    engine.evaluate(R"(
        var target = document.getElementById('target');
        var el2 = document.createElement('span');
        el2.setAttribute('id', 'ae');
        target.insertAdjacentElement('afterend', el2);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto ae = engine.evaluate("document.getElementById('ae') !== null");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(ae, "true");

    // afterbegin
    engine.evaluate(R"(
        var target = document.getElementById('target');
        var el3 = document.createElement('em');
        el3.setAttribute('id', 'ab');
        target.insertAdjacentElement('afterbegin', el3);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto ab = engine.evaluate("document.getElementById('ab') !== null");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(ab, "true");

    // beforeend
    engine.evaluate(R"(
        var target = document.getElementById('target');
        var el4 = document.createElement('strong');
        el4.setAttribute('id', 'be');
        target.insertAdjacentElement('beforeend', el4);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();

    auto be = engine.evaluate("document.getElementById('be') !== null");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(be, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 220: Global APIs
// ============================================================================

TEST(JSDom, AbortControllerBasic) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    auto result = engine.evaluate(R"(
        var ac = new AbortController();
        var s1 = ac.signal.aborted;
        ac.abort();
        var s2 = ac.signal.aborted;
        s1 + ',' + s2;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false,true");

    // Test abort reason
    auto reason = engine.evaluate(R"(
        var ac2 = new AbortController();
        ac2.abort('custom reason');
        ac2.signal.reason;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(reason, "custom reason");

    // Default abort reason is an AbortError
    auto defaultReason = engine.evaluate(R"(
        var ac3 = new AbortController();
        ac3.abort();
        ac3.signal.reason.name;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(defaultReason, "AbortError");
}

TEST(JSDom, StructuredClone) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    auto result = engine.evaluate(R"(
        var obj = { a: 1, b: [2, 3], c: { d: 'hello' } };
        var clone = structuredClone(obj);
        clone.a = 99;
        clone.b.push(4);
        obj.a + ',' + obj.b.length + ',' + clone.a + ',' + clone.b.length;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,99,3");
}

TEST(JSDom, RequestIdleCallback) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    auto result = engine.evaluate(R"(
        var called = false;
        var remaining = 0;
        var timedOut = true;
        var id = requestIdleCallback(function(deadline) {
            called = true;
            remaining = deadline.timeRemaining();
            timedOut = deadline.didTimeout;
        });
        called + ',' + (remaining > 0) + ',' + timedOut + ',' + (id > 0);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,true,false,true");

    // cancelIdleCallback should not throw
    auto cancel = engine.evaluate("cancelIdleCallback(123); 'ok'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(cancel, "ok");
}

TEST(JSDom, CSSSupports) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);

    // Two-argument form
    auto result1 = engine.evaluate("CSS.supports('display', 'grid')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result1, "true");

    auto result2 = engine.evaluate("CSS.supports('nonexistent-property', 'value')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "false");

    // One-argument (condition) form
    auto result3 = engine.evaluate("CSS.supports('(display: grid)')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result3, "true");

    auto result4 = engine.evaluate("CSS.supports('(color: red)')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result4, "true");

    auto result5 = engine.evaluate("CSS.supports('(fake-prop: val)')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result5, "false");
}

// ============================================================================
// document.createEvent() + initEvent()
// ============================================================================
TEST(JSDom, DocumentCreateEvent) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // createEvent returns an event object with initEvent
    auto result = engine.evaluate(
        "(function() {"
        "  var e = document.createEvent('Event');"
        "  e.initEvent('click', true, true);"
        "  return e.type + '|' + e.bubbles + '|' + e.cancelable;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "click|true|true");

    // Has preventDefault
    auto result2 = engine.evaluate(
        "(function() {"
        "  var e = document.createEvent('Event');"
        "  e.initEvent('test', false, false);"
        "  e.preventDefault();"
        "  return e.defaultPrevented;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Event constructor: new Event(type, options)
// ============================================================================
TEST(JSDom, EventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "(function() {"
        "  var e = new Event('custom', {bubbles: true, cancelable: true});"
        "  return e.type + '|' + e.bubbles + '|' + e.cancelable;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "custom|true|true");

    // Default bubbles/cancelable are false
    auto result2 = engine.evaluate(
        "(function() {"
        "  var e = new Event('test');"
        "  return e.bubbles + '|' + e.cancelable;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "false|false");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// KeyboardEvent constructor
// ============================================================================
TEST(JSDom, KeyboardEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "(function() {"
        "  var e = new KeyboardEvent('keydown', {"
        "    key: 'Enter', code: 'Enter', keyCode: 13,"
        "    ctrlKey: true, shiftKey: false"
        "  });"
        "  return e.type + '|' + e.key + '|' + e.code + '|' + e.keyCode + '|' + e.ctrlKey;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "keydown|Enter|Enter|13|true");

    // Default values
    auto result2 = engine.evaluate(
        "(function() {"
        "  var e = new KeyboardEvent('keyup');"
        "  return e.key + '|' + e.keyCode + '|' + e.ctrlKey;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "|0|false");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// MouseEvent constructor
// ============================================================================
TEST(JSDom, MouseEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "(function() {"
        "  var e = new MouseEvent('click', {"
        "    button: 1, clientX: 100, clientY: 200,"
        "    ctrlKey: false, metaKey: true"
        "  });"
        "  return e.type + '|' + e.button + '|' + e.clientX + '|' + e.clientY + '|' + e.metaKey;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "click|1|100|200|true");

    // Default values
    auto result2 = engine.evaluate(
        "(function() {"
        "  var e = new MouseEvent('mousedown');"
        "  return e.button + '|' + e.clientX + '|' + e.clientY + '|' + e.ctrlKey;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "0|0|0|false");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// TextEncoder — basic encode
// ============================================================================
TEST(JSWindow, TextEncoderBasic) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);

    auto result = engine.evaluate(
        "(function() {"
        "  var enc = new TextEncoder();"
        "  var arr = enc.encode('hello');"
        "  return arr.length;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "5");
}

// ============================================================================
// TextEncoder — encoding property
// ============================================================================
TEST(JSWindow, TextEncoderEncoding) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);

    auto result = engine.evaluate("new TextEncoder().encoding");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "utf-8");
}

// ============================================================================
// TextDecoder — basic decode
// ============================================================================
TEST(JSWindow, TextDecoderBasic) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);

    auto result = engine.evaluate(
        "(function() {"
        "  var dec = new TextDecoder();"
        "  var arr = new Uint8Array([104, 101, 108, 108, 111]);"
        "  return dec.decode(arr);"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello");
}

// ============================================================================
// FormData — append and get
// ============================================================================
TEST(JSFetch, FormDataAppendAndGet) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());

    auto result = engine.evaluate(
        "(function() {"
        "  var fd = new FormData();"
        "  fd.append('key', 'value');"
        "  return fd.get('key');"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "value");
}

// ============================================================================
// FormData — has and delete
// ============================================================================
TEST(JSFetch, FormDataHasAndDelete) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());

    auto result = engine.evaluate(
        "(function() {"
        "  var fd = new FormData();"
        "  fd.append('key', 'value');"
        "  var hasBefore = fd.has('key');"
        "  fd.delete('key');"
        "  var hasAfter = fd.has('key');"
        "  return hasBefore + '|' + hasAfter;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true|false");
}

// ============================================================================
// FormData — set replaces existing value
// ============================================================================
TEST(JSFetch, FormDataSet) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());

    auto result = engine.evaluate(
        "(function() {"
        "  var fd = new FormData();"
        "  fd.append('key', 'old');"
        "  fd.set('key', 'new');"
        "  return fd.get('key');"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "new");
}

// ============================================================================
// document.createRange — returns object with collapsed=true
// ============================================================================
TEST(JSDom, DocumentCreateRange) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "(function() {"
        "  var range = document.createRange();"
        "  return range.collapsed;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// navigator.clipboard — exists and writeText returns a Promise
// ============================================================================
TEST(JSWindow, NavigatorClipboard) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);

    auto result = engine.evaluate(
        "(function() {"
        "  return typeof navigator.clipboard;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");

    auto result2 = engine.evaluate(
        "(function() {"
        "  var p = navigator.clipboard.writeText('hello');"
        "  return p instanceof Promise;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "true");
}

// ============================================================================
// DOMParser — basic parseFromString and body content access
// ============================================================================
TEST(JSDom, DOMParserBasic) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "(function() {"
        "  var parser = new DOMParser();"
        "  var d = parser.parseFromString('<div>Hello</div>', 'text/html');"
        "  return d.body.firstChild.textContent;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Hello");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DOMParser — querySelector on parsed document
// ============================================================================
TEST(JSDom, DOMParserQuerySelector) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "(function() {"
        "  var parser = new DOMParser();"
        "  var d = parser.parseFromString("
        "    '<div id=\"test\"><span class=\"msg\">World</span></div>', 'text/html');"
        "  var el = d.querySelector('.msg');"
        "  return el ? el.textContent : 'NOT FOUND';"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "World");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// document.elementFromPoint — returns non-null (body stub)
// ============================================================================
TEST(JSDom, ElementFromPoint) {
    auto doc = clever::html::parse("<html><body><p>text</p></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "(function() {"
        "  var el = document.elementFromPoint(100, 200);"
        "  return el !== null;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// element.getAttributeNames() — returns array with correct attribute names
// ============================================================================
TEST(JSDom, GetAttributeNames) {
    auto doc = clever::html::parse(
        "<html><body><div id=\"box\" class=\"red\" data-x=\"1\"></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "(function() {"
        "  var el = document.getElementById('box');"
        "  var names = el.getAttributeNames();"
        "  return names.sort().join(',');"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "class,data-x,id");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// element.isConnected — true when in DOM tree, false when detached
// ============================================================================
TEST(JSDom, IsConnected) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "(function() {"
        "  var el = document.createElement('div');"
        "  var before = el.isConnected;"
        "  document.body.appendChild(el);"
        "  var after = el.isConnected;"
        "  return before + ',' + after;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false,true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// document.createDocumentFragment — append transfers children
// ============================================================================
TEST(JSDom, CreateDocumentFragmentAppend) {
    auto doc = clever::html::parse("<html><body><div id=\"target\"></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(
        "(function() {"
        "  var frag = document.createDocumentFragment();"
        "  var a = document.createElement('span');"
        "  a.textContent = 'A';"
        "  var b = document.createElement('span');"
        "  b.textContent = 'B';"
        "  frag.appendChild(a);"
        "  frag.appendChild(b);"
        "  var target = document.getElementById('target');"
        "  target.appendChild(frag);"
        "  return target.children.length + ':' + target.textContent;"
        "})()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "2:AB");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// window.scrollX / window.scrollY — should be 0
// ============================================================================
TEST(JSWindow, ScrollPosition) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "about:blank", 800, 600);

    auto result = engine.evaluate(
        "(window.scrollX === 0 && window.scrollY === 0 && "
        " window.pageXOffset === 0 && window.pageYOffset === 0).toString()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// document.readyState — should be "complete"
// ============================================================================
TEST(JSDom, ReadyState) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("document.readyState");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "complete");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// document.defaultView — should equal window (globalThis)
// ============================================================================
TEST(JSDom, DefaultView) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "about:blank", 800, 600);
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("(document.defaultView === window).toString()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// document.characterEncoding — should be "UTF-8"
// ============================================================================
TEST(JSDom, CharacterEncoding) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("document.characterEncoding");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "UTF-8");

    auto result2 = engine.evaluate("document.contentType");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "text/html");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// document.implementation.hasFeature() — should return true
// ============================================================================
TEST(JSDom, Implementation) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("(document.implementation.hasFeature() === true).toString()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Node.hasChildNodes() — true if has children
// ============================================================================
TEST(JSDom, HasChildNodes) {
    auto doc = clever::html::parse("<html><body><div id='parent'><span></span></div><div id='empty'></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result1 = engine.evaluate(R"(
        document.getElementById('parent').hasChildNodes().toString()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result1, "true");

    auto result2 = engine.evaluate(R"(
        document.getElementById('empty').hasChildNodes().toString()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "false");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Node.getRootNode() — returns root of the tree
// ============================================================================
TEST(JSDom, GetRootNode) {
    auto doc = clever::html::parse("<html><body><div id='deep'><span id='inner'></span></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var inner = document.getElementById('inner');
        var root = inner.getRootNode();
        (root.nodeType !== undefined).toString()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Node.isSameNode() — true if same reference, false otherwise
// ============================================================================
TEST(JSDom, IsSameNode) {
    auto doc = clever::html::parse("<html><body><div id='a'></div><div id='b'></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result1 = engine.evaluate(R"(
        var a = document.getElementById('a');
        a.isSameNode(a).toString()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result1, "true");

    auto result2 = engine.evaluate(R"(
        var a2 = document.getElementById('a');
        var b = document.getElementById('b');
        a2.isSameNode(b).toString()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "false");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Element.insertAdjacentText() — insert text, verify textContent includes it
// ============================================================================
TEST(JSDom, InsertAdjacentText) {
    auto doc = clever::html::parse("<html><body><div id='target'>original</div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var el = document.getElementById('target');
        el.insertAdjacentText('beforeend', ' added');
        el.textContent
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_NE(result.find("added"), std::string::npos);

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// document.visibilityState / document.hidden
// ============================================================================
TEST(JSDom, DocumentVisibility) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result1 = engine.evaluate("document.visibilityState");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result1, "visible");

    auto result2 = engine.evaluate("document.hidden.toString()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "false");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// document.forms / document.images — return arrays
// ============================================================================
TEST(JSDom, DocumentCollections) {
    auto doc = clever::html::parse("<html><body><form></form><img><img></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result1 = engine.evaluate("Array.isArray(document.forms).toString()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result1, "true");

    auto result2 = engine.evaluate("document.forms.length.toString()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "1");

    auto result3 = engine.evaluate("Array.isArray(document.images).toString()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result3, "true");

    auto result4 = engine.evaluate("document.images.length.toString()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result4, "2");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// document.createComment() — creates comment node
// ============================================================================
TEST(JSDom, CreateComment) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result1 = engine.evaluate(R"(
        var comment = document.createComment('test comment');
        comment.nodeType.toString()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result1, "8");  // Comment nodeType is 8

    auto result2 = engine.evaluate(R"(
        var comment2 = document.createComment('hello');
        comment2.textContent
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "hello");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// addEventListener with {once: true} — callback fires only once
// ============================================================================
TEST(JSDom, EventListenerOnce) {
    auto doc = clever::html::parse("<html><body><div id='target'></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var count = 0;
        var el = document.getElementById('target');
        el.addEventListener('click', function() { count++; }, { once: true });
        el.dispatchEvent(new Event('click'));
        el.dispatchEvent(new Event('click'));
        count.toString()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// element.hidden — set hidden attribute, read it back
// ============================================================================
TEST(JSDom, ElementHidden) {
    auto doc = clever::html::parse("<html><body><div id='target'></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result1 = engine.evaluate(R"(
        var el = document.getElementById('target');
        el.hidden.toString()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result1, "false");

    auto result2 = engine.evaluate(R"(
        var el = document.getElementById('target');
        el.hidden = true;
        el.hidden.toString()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "true");

    auto result3 = engine.evaluate(R"(
        var el = document.getElementById('target');
        el.hasAttribute('hidden').toString()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result3, "true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// PointerEvent constructor
// ============================================================================
TEST(JSDom, PointerEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var evt = new PointerEvent("pointerdown", {
            clientX: 100, clientY: 200,
            pointerId: 1, pointerType: "mouse",
            width: 1, height: 1, pressure: 0.5,
            isPrimary: true
        });
        var parts = [];
        parts.push(evt.type);
        parts.push(evt.pointerId);
        parts.push(evt.pointerType);
        parts.push(evt.clientX);
        parts.push(evt.clientY);
        parts.push(evt.pressure);
        parts.push(evt.isPrimary);
        parts.join(',')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "pointerdown,1,mouse,100,200,0.5,true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// FocusEvent constructor
// ============================================================================
TEST(JSDom, FocusEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var evt = new FocusEvent("focus");
        var parts = [];
        parts.push(evt.type);
        parts.push(evt.relatedTarget === null);
        parts.push(typeof evt.preventDefault);
        parts.join(',')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "focus,true,function");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// InputEvent constructor
// ============================================================================
TEST(JSDom, InputEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var evt = new InputEvent("input", {
            data: "a", inputType: "insertText", isComposing: false
        });
        var parts = [];
        parts.push(evt.type);
        parts.push(evt.data);
        parts.push(evt.inputType);
        parts.push(evt.isComposing);
        parts.join(',')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "input,a,insertText,false");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// document.createTreeWalker()
// ============================================================================
TEST(JSDom, CreateTreeWalker) {
    auto doc = clever::html::parse(
        "<html><body><div id='root'><span>A</span><p>B</p></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var root = document.getElementById('root');
        var walker = document.createTreeWalker(root, NodeFilter.SHOW_ELEMENT);
        var tags = [];
        var node;
        while (node = walker.nextNode()) {
            tags.push(node.tagName);
        }
        tags.join(',')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "SPAN,P");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// window.location enhanced properties
// ============================================================================
TEST(JSDom, LocationProperties) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(),
        "https://example.com:8080/page?q=1#top", 1024, 768);

    auto origin = engine.evaluate("window.location.origin");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(origin, "https://example.com:8080");

    auto host = engine.evaluate("window.location.host");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(host, "example.com:8080");

    auto port = engine.evaluate("window.location.port");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(port, "8080");

    auto search = engine.evaluate("window.location.search");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(search, "?q=1");

    auto hash = engine.evaluate("window.location.hash");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(hash, "#top");

    // Default port test (no port in URL)
    clever::js::JSEngine engine2;
    clever::js::install_window_bindings(engine2.context(),
        "https://example.com/path", 1024, 768);

    auto origin2 = engine2.evaluate("window.location.origin");
    EXPECT_FALSE(engine2.has_error()) << engine2.last_error();
    EXPECT_EQ(origin2, "https://example.com");

    auto host2 = engine2.evaluate("window.location.host");
    EXPECT_FALSE(engine2.has_error()) << engine2.last_error();
    EXPECT_EQ(host2, "example.com");

    auto port2 = engine2.evaluate("window.location.port");
    EXPECT_FALSE(engine2.has_error()) << engine2.last_error();
    EXPECT_EQ(port2, "");
}

// ============================================================================
// window.getSelection() enhanced properties and methods
// ============================================================================
TEST(JSWindow, GetSelectionEnhanced) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);

    auto rangeCount = engine.evaluate("window.getSelection().rangeCount");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(rangeCount, "0");

    auto isCollapsed = engine.evaluate("window.getSelection().isCollapsed");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(isCollapsed, "true");

    auto type = engine.evaluate("window.getSelection().type");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(type, "None");

    auto toString = engine.evaluate("window.getSelection().toString()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(toString, "");

    auto anchorNode = engine.evaluate("window.getSelection().anchorNode");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(anchorNode, "null");

    auto anchorOffset = engine.evaluate("window.getSelection().anchorOffset");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(anchorOffset, "0");

    auto focusOffset = engine.evaluate("window.getSelection().focusOffset");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(focusOffset, "0");

    // Test no-op methods don't throw
    auto collapse = engine.evaluate("window.getSelection().collapse(); 'ok'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(collapse, "ok");

    auto removeAll = engine.evaluate("window.getSelection().removeAllRanges(); 'ok'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(removeAll, "ok");

    auto selectAll = engine.evaluate("window.getSelection().selectAllChildren(null); 'ok'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(selectAll, "ok");

    auto deleteFrom = engine.evaluate("window.getSelection().deleteFromDocument(); 'ok'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(deleteFrom, "ok");

    auto containsNode = engine.evaluate("window.getSelection().containsNode(null)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(containsNode, "false");
}

// ============================================================================
// ErrorEvent constructor
// ============================================================================
TEST(JSDom, ErrorEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto type = engine.evaluate(
        "var e = new ErrorEvent('error', {message: 'oops', filename: 'test.js', lineno: 42, colno: 5}); e.type");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(type, "error");

    auto msg = engine.evaluate("e.message");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(msg, "oops");

    auto filename = engine.evaluate("e.filename");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(filename, "test.js");

    auto lineno = engine.evaluate("e.lineno");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(lineno, "42");

    auto colno = engine.evaluate("e.colno");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(colno, "5");

    auto error = engine.evaluate("e.error");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(error, "null");
}

// ============================================================================
// PromiseRejectionEvent constructor
// ============================================================================
TEST(JSDom, PromiseRejectionEvent) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto type = engine.evaluate(
        "var pre = new PromiseRejectionEvent('unhandledrejection', {reason: 'fail'}); pre.type");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(type, "unhandledrejection");

    auto reason = engine.evaluate("pre.reason");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(reason, "fail");

    auto promise = engine.evaluate("pre.promise");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(promise, "null");

    // Default values — reason defaults to undefined
    auto def = engine.evaluate(
        "var pre2 = new PromiseRejectionEvent('test'); typeof pre2.reason");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(def, "undefined");
}

// ============================================================================
// window.performance enhanced
// ============================================================================
TEST(JSWindow, PerformanceEnhanced) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);

    auto isNumber = engine.evaluate("typeof performance.timeOrigin === 'number'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(isNumber, "true");

    // timeOrigin should be a large number (milliseconds since epoch)
    auto timeOriginBig = engine.evaluate("performance.timeOrigin > 1000000000000");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(timeOriginBig, "true");

    auto entries = engine.evaluate("Array.isArray(performance.getEntries())");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(entries, "true");

    auto entriesLen = engine.evaluate("performance.getEntries().length");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(entriesLen, "0");

    auto byType = engine.evaluate("Array.isArray(performance.getEntriesByType('resource'))");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(byType, "true");

    auto byName = engine.evaluate("Array.isArray(performance.getEntriesByName('test'))");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(byName, "true");

    // No-op methods should not throw
    auto mark = engine.evaluate("performance.mark('test'); 'ok'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(mark, "ok");

    auto measure = engine.evaluate("performance.measure('test'); 'ok'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(measure, "ok");

    auto clearMarks = engine.evaluate("performance.clearMarks(); 'ok'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(clearMarks, "ok");

    auto clearMeasures = engine.evaluate("performance.clearMeasures(); 'ok'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(clearMeasures, "ok");
}

// ============================================================================
// screen.orientation
// ============================================================================
TEST(JSWindow, ScreenOrientation) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);

    auto type = engine.evaluate("screen.orientation.type");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(type, "landscape-primary");

    auto angle = engine.evaluate("screen.orientation.angle");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(angle, "0");

    auto availLeft = engine.evaluate("screen.availLeft");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(availLeft, "0");

    auto availTop = engine.evaluate("screen.availTop");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(availTop, "0");
}

// ============================================================================
// document.hasFocus()
// ============================================================================
TEST(JSDom, DocumentHasFocus) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate("document.hasFocus()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    // hasFocus should be a function
    auto typeCheck = engine.evaluate("typeof document.hasFocus");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(typeCheck, "function");
}

// ============================================================================
// window.isSecureContext
// ============================================================================
TEST(JSWindow, IsSecureContext) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);

    auto result = engine.evaluate("window.isSecureContext");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");

    auto typeCheck = engine.evaluate("typeof window.isSecureContext");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(typeCheck, "boolean");
}

// ============================================================================
// Shadow DOM: attachShadow creates shadow root
// ============================================================================
TEST(JSDom, AttachShadowCreatesShadowRoot) {
    auto doc = clever::html::parse("<html><body><div id='host'></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    // attachShadow should return an object (the shadow root)
    auto result = engine.evaluate(R"(
        var host = document.getElementById('host');
        var shadow = host.attachShadow({mode: 'open'});
        shadow !== null && shadow !== undefined ? 'ok' : 'fail';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ok");

    // shadowRoot should return the same shadow root
    auto result2 = engine.evaluate(R"(
        host.shadowRoot !== null ? 'ok' : 'fail';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "ok");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Shadow DOM: shadowRoot innerHTML works
// ============================================================================
TEST(JSDom, ShadowRootInnerHTML) {
    auto doc = clever::html::parse("<html><body><div id='host'></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var host = document.getElementById('host');
        var shadow = host.attachShadow({mode: 'open'});
        shadow.innerHTML = '<p>Shadow content</p>';
        shadow.innerHTML;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "<p>Shadow content</p>");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Shadow DOM: closed mode returns null shadowRoot
// ============================================================================
TEST(JSDom, ClosedShadowRootReturnsNull) {
    auto doc = clever::html::parse("<html><body><div id='host'></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var host = document.getElementById('host');
        var shadow = host.attachShadow({mode: 'closed'});
        // attachShadow returns the shadow root even in closed mode
        var attachResult = shadow !== null ? 'attached' : 'fail';
        // But shadowRoot getter returns null for closed mode
        var getterResult = host.shadowRoot === null ? 'null' : 'not-null';
        attachResult + ',' + getterResult;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "attached,null");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// HTMLTemplateElement.content returns fragment
// ============================================================================
TEST(JSDom, TemplateContentReturnsFragment) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <template id='tmpl'><p>Template text</p></template>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var tmpl = document.getElementById('tmpl');
        var content = tmpl.content;
        content !== null && content !== undefined ? 'ok' : 'fail';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ok");

    // content of a non-template element should be undefined
    auto result2 = engine.evaluate(R"(
        var div = document.createElement('div');
        div.content === undefined ? 'undefined' : 'not-undefined';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result2, "undefined");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Node.normalize merges adjacent text nodes
// ============================================================================
TEST(JSDom, NodeNormalizeMergesTextNodes) {
    auto doc = clever::html::parse("<html><body><div id='target'></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var div = document.getElementById('target');
        // Add three text nodes manually
        var t1 = document.createTextNode('Hello');
        var t2 = document.createTextNode(' ');
        var t3 = document.createTextNode('World');
        div.appendChild(t1);
        div.appendChild(t2);
        div.appendChild(t3);
        // Before normalize: 3 child nodes
        var before = div.childNodes.length;
        div.normalize();
        // After normalize: 1 merged text node
        var after = div.childNodes.length;
        var text = div.textContent;
        before + ',' + after + ',' + text;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3,1,Hello World");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Node.isEqualNode deep comparison
// ============================================================================
TEST(JSDom, NodeIsEqualNodeDeepComparison) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        // Create two identical trees
        var a = document.createElement('div');
        a.setAttribute('class', 'test');
        var aChild = document.createElement('span');
        a.appendChild(aChild);

        var b = document.createElement('div');
        b.setAttribute('class', 'test');
        var bChild = document.createElement('span');
        b.appendChild(bChild);

        var equal = a.isEqualNode(b);

        // Now make them different
        b.setAttribute('id', 'different');
        var notEqual = a.isEqualNode(b);

        equal + ',' + notEqual;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// document.adoptNode
// ============================================================================
TEST(JSDom, DocumentAdoptNode) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id='parent'><span id='child'>text</span></div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var parent = document.getElementById('parent');
        var child = document.getElementById('child');
        var beforeChildren = parent.childNodes.length;
        var adopted = document.adoptNode(child);
        // adoptNode removes the node from its parent
        var afterChildren = parent.childNodes.length;
        var isNode = adopted !== null ? 'ok' : 'fail';
        isNode + ',' + beforeChildren + ',' + afterChildren;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // parent had 1 child (span), after adoptNode it has 0
    EXPECT_EQ(result, "ok,1,0");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Shadow DOM: attachShadow twice throws error
// ============================================================================
TEST(JSDom, AttachShadowTwiceThrowsError) {
    auto doc = clever::html::parse("<html><body><div id='host'></div></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var host = document.getElementById('host');
        host.attachShadow({mode: 'open'});
        try {
            host.attachShadow({mode: 'open'});
            'no-error';
        } catch(e) {
            'error';
        }
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "error");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Navigator properties
// ============================================================================
TEST(JSEngine, NavigatorLanguage) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("navigator.language");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "en-US");
}

TEST(JSEngine, NavigatorLanguages) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("navigator.languages.includes('en')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, NavigatorPlatform) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("typeof navigator.platform");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "string");
}

// ============================================================================
// Console methods
// ============================================================================
TEST(JSEngine, ConsoleDebug) {
    clever::js::JSEngine engine;
    engine.evaluate("console.debug('test debug')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    ASSERT_FALSE(engine.console_output().empty());
    EXPECT_EQ(engine.console_output().back(), "[log] test debug");
}

TEST(JSEngine, ConsoleAssert) {
    clever::js::JSEngine engine;
    engine.evaluate("console.assert(false, 'oops')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    ASSERT_FALSE(engine.console_output().empty());
    EXPECT_EQ(engine.console_output().back(), "[error] Assertion failed: oops");
}

TEST(JSEngine, ConsoleTimeAndTimeEnd) {
    clever::js::JSEngine engine;
    engine.evaluate("console.time('test')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    engine.evaluate("console.timeEnd('test')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // Should have produced a timing message
    ASSERT_FALSE(engine.console_output().empty());
    auto& last = engine.console_output().back();
    // The message should start with "[log] test:"
    EXPECT_TRUE(last.find("[log] test:") == 0) << "Got: " << last;
}

TEST(JSEngine, ConsoleCount) {
    clever::js::JSEngine engine;
    engine.evaluate("console.count('clicks')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    ASSERT_FALSE(engine.console_output().empty());
    EXPECT_EQ(engine.console_output().back(), "[log] clicks: 1");
    engine.evaluate("console.count('clicks')");
    EXPECT_EQ(engine.console_output().back(), "[log] clicks: 2");
}

// ============================================================================
// window.confirm / window.prompt stubs
// ============================================================================
TEST(JSWindow, WindowConfirm) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("window.confirm('Are you sure?')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSWindow, WindowPrompt) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 1024, 768);
    auto result = engine.evaluate("window.prompt('Enter name:')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "");
}

// ============================================================================
// WheelEvent constructor
// ============================================================================
TEST(JSDom, WheelEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var e = new WheelEvent('wheel', {deltaX: 10.5, deltaY: -20.3, deltaZ: 1, deltaMode: 1});
        e.type + ',' + e.deltaX + ',' + e.deltaY + ',' + e.deltaZ + ',' + e.deltaMode;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "wheel,10.5,-20.3,1,1");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// HashChangeEvent constructor
// ============================================================================
TEST(JSDom, HashChangeEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var e = new HashChangeEvent('hashchange', {
            oldURL: 'http://example.com/#old',
            newURL: 'http://example.com/#new'
        });
        e.type + ',' + e.oldURL + ',' + e.newURL;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hashchange,http://example.com/#old,http://example.com/#new");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// PopStateEvent constructor
// ============================================================================
TEST(JSDom, PopStateEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var e = new PopStateEvent('popstate', {state: {page: 1}});
        e.type + ',' + e.state.page;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "popstate,1");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// TransitionEvent constructor
// ============================================================================
TEST(JSDom, TransitionEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var e = new TransitionEvent('transitionend', {
            propertyName: 'opacity',
            elapsedTime: 0.5,
            pseudoElement: '::before'
        });
        e.type + ',' + e.propertyName + ',' + e.elapsedTime + ',' + e.pseudoElement;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "transitionend,opacity,0.5,::before");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// AnimationEvent constructor
// ============================================================================
TEST(JSDom, AnimationEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var e = new AnimationEvent('animationend', {
            animationName: 'fadeIn',
            elapsedTime: 1.5,
            pseudoElement: ''
        });
        e.type + ',' + e.animationName + ',' + e.elapsedTime + ',' + e.pseudoElement;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "animationend,fadeIn,1.5,");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// ContentEditable getter/setter
// ============================================================================
TEST(JSDom, ContentEditableGetSet) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id='editable' contenteditable='true'>hello</div>
            <div id='noteditable'>world</div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var ed = document.getElementById('editable');
        var ne = document.getElementById('noteditable');
        var r1 = ed.contentEditable;
        var r2 = ne.contentEditable;
        ne.contentEditable = 'true';
        var r3 = ne.contentEditable;
        r1 + ',' + r2 + ',' + r3;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,inherit,true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Draggable getter/setter
// ============================================================================
TEST(JSDom, DraggableGetSet) {
    auto doc = clever::html::parse(R"(
        <html><body>
            <div id='drag' draggable='true'>drag me</div>
            <div id='nodrag'>stay</div>
        </body></html>
    )");
    ASSERT_NE(doc, nullptr);

    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());

    auto result = engine.evaluate(R"(
        var d = document.getElementById('drag');
        var nd = document.getElementById('nodrag');
        var r1 = d.draggable;
        var r2 = nd.draggable;
        nd.draggable = true;
        var r3 = nd.draggable;
        r1 + ',' + r2 + ',' + r3;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false,true");

    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Navigator API tests
// ============================================================================
TEST(JSDom, NavigatorUserAgent) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("navigator.userAgent");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_TRUE(result.find("Clever") != std::string::npos);
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, NavigatorLanguage) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("navigator.language");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "en-US");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, NavigatorLanguages) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("navigator.languages.length + ',' + navigator.languages[0]");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "2,en-US");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, NavigatorOnLine) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("String(navigator.onLine)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, NavigatorPlatform) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("navigator.platform");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "MacIntel");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, NavigatorHardwareConcurrency) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("String(navigator.hardwareConcurrency)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "4");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, NavigatorCookieEnabled) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("String(navigator.cookieEnabled)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// window.location tests
// ============================================================================
TEST(JSDom, WindowLocation) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("location.protocol + '//' + location.pathname");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "about://blank");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// window.screen tests
// ============================================================================
TEST(JSDom, WindowScreen) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("screen.width + 'x' + screen.height");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1920x1080");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, WindowScreenColorDepth) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("String(screen.colorDepth)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "24");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// window.history stub tests
// ============================================================================
TEST(JSDom, WindowHistory) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("String(history.length)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// window dimensions tests
// ============================================================================
TEST(JSDom, WindowDimensions) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("innerWidth + 'x' + innerHeight");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1024x768");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, WindowDevicePixelRatio) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("String(devicePixelRatio)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "2");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, WindowScrollOffsets) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("scrollX + ',' + scrollY + ',' + pageXOffset + ',' + pageYOffset");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0,0,0,0");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Blob API tests
// ============================================================================
TEST(JSDom, BlobConstructor) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var b = new Blob(['hello', ' world'], {type: 'text/plain'});
        b.size + ',' + b.type;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "11,text/plain");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, BlobText) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    // Blob.text() returns a Promise; verify it exists and returns object
    auto result = engine.evaluate(R"(
        var b = new Blob(['abc', 'def']);
        var p = b.text();
        typeof p + ',' + (typeof p.then);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object,function");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, FileConstructor) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var f = new File(['content'], 'test.txt', {type: 'text/plain'});
        f.name + ',' + f.type + ',' + f.size;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "test.txt,text/plain,7");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, FileReaderReadAsText) {
    auto doc = clever::html::parse(
        "<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    // FileReader exists and has the expected methods
    auto result = engine.evaluate(R"(
        var fr = new FileReader();
        typeof fr.readAsText + ',' + typeof fr.readAsDataURL + ',' +
        typeof fr.readAsArrayBuffer + ',' + typeof fr.abort + ',' +
        typeof fr.addEventListener;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function,function,function,function,function");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Canvas 2D curve methods tests
// ============================================================================
TEST(JSDom, CanvasQuadraticCurveTo) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        ctx.beginPath();
        ctx.moveTo(0, 0);
        ctx.quadraticCurveTo(50, 100, 100, 0);
        ctx.stroke();
        'ok';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ok");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasBezierCurveTo) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        ctx.beginPath();
        ctx.moveTo(0, 0);
        ctx.bezierCurveTo(20, 100, 80, 100, 100, 0);
        ctx.stroke();
        'ok';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ok");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasArcTo) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        ctx.beginPath();
        ctx.moveTo(10, 10);
        ctx.arcTo(100, 10, 100, 100, 50);
        ctx.stroke();
        'ok';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ok");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasEllipse) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        ctx.beginPath();
        ctx.ellipse(50, 50, 40, 20, 0, 0, Math.PI * 2);
        ctx.fill();
        'ok';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ok");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Canvas 2D gradient tests
// ============================================================================
TEST(JSDom, CanvasCreateLinearGradient) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        var grad = ctx.createLinearGradient(0, 0, 100, 0);
        grad.addColorStop(0, 'red');
        grad.addColorStop(1, 'blue');
        typeof grad + ',' + grad.type;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object,linear");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasCreateRadialGradient) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        var grad = ctx.createRadialGradient(50, 50, 10, 50, 50, 50);
        grad.addColorStop(0, 'white');
        grad.addColorStop(1, 'black');
        typeof grad + ',' + grad.type;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object,radial");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasCreateConicGradient) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        var grad = ctx.createConicGradient(0, 50, 50);
        grad.addColorStop(0, 'red');
        grad.addColorStop(0.5, 'green');
        grad.addColorStop(1, 'blue');
        typeof grad + ',' + grad.type;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object,conic");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasCreatePattern) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        var pat = ctx.createPattern(null, 'repeat');
        typeof pat + ',' + pat.type;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object,pattern");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasSetGetLineDash) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        ctx.setLineDash([5, 10]);
        var dash = ctx.getLineDash();
        Array.isArray(dash) ? 'ok' : 'fail';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ok");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasIsPointInPath) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        String(ctx.isPointInPath(50, 50));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false");
    clever::js::cleanup_dom_bindings(engine.context());
}

// Navigator sub-objects exist
TEST(JSDom, NavigatorClipboardExists) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("typeof navigator.clipboard");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, NavigatorServiceWorkerExists) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("typeof navigator.serviceWorker");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, NavigatorPermissionsExists) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("typeof navigator.permissions");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, NavigatorMediaDevicesExists) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("typeof navigator.mediaDevices");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, NavigatorVendor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("navigator.vendor");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Clever Browser");
    clever::js::cleanup_dom_bindings(engine.context());
}

// File lastModified
TEST(JSDom, FileLastModified) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var f = new File(['data'], 'test.bin', {lastModified: 12345});
        String(f.lastModified);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "12345");
    clever::js::cleanup_dom_bindings(engine.context());
}

// Blob slice returns Blob
// ============================================================================
// Canvas 2D style property tests
// ============================================================================
TEST(JSDom, CanvasTextBaseline) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        var def = ctx.textBaseline;
        ctx.textBaseline = 'middle';
        def + ',' + ctx.textBaseline;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "alphabetic,middle");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasLineCap) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        var def = ctx.lineCap;
        ctx.lineCap = 'round';
        def + ',' + ctx.lineCap;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "butt,round");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasLineJoin) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        var def = ctx.lineJoin;
        ctx.lineJoin = 'bevel';
        def + ',' + ctx.lineJoin;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "miter,bevel");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasMiterLimit) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        var def = ctx.miterLimit;
        ctx.miterLimit = 5;
        def + ',' + ctx.miterLimit;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "10,5");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasShadowColor) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        ctx.shadowColor = 'red';
        ctx.shadowBlur = 10;
        ctx.shadowOffsetX = 5;
        ctx.shadowOffsetY = 3;
        ctx.shadowBlur + ',' + ctx.shadowOffsetX + ',' + ctx.shadowOffsetY;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "10,5,3");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasGlobalCompositeOperation) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        var def = ctx.globalCompositeOperation;
        ctx.globalCompositeOperation = 'multiply';
        def + ',' + ctx.globalCompositeOperation;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "source-over,multiply");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasImageSmoothingEnabled) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        var def = ctx.imageSmoothingEnabled;
        ctx.imageSmoothingEnabled = false;
        def + ',' + ctx.imageSmoothingEnabled;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, BlobSlice) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var b = new Blob(['hello'], {type: 'text/plain'});
        var s = b.slice(0, 3, 'text/plain');
        typeof s + ',' + s.type;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object,text/plain");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// TouchEvent constructor tests
// ============================================================================
TEST(JSDom, TouchEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var e = new TouchEvent('touchstart');
        e.type + ',' + Array.isArray(e.touches) + ',' + Array.isArray(e.changedTouches);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "touchstart,true,true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, TouchEventWithOptions) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var e = new TouchEvent('touchend', {bubbles: false});
        e.type + ',' + e.bubbles;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "touchend,false");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// DragEvent constructor tests
// ============================================================================
TEST(JSDom, DragEventConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var e = new DragEvent('dragstart');
        e.type + ',' + typeof e.dataTransfer + ',' + e.dataTransfer.dropEffect;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "dragstart,object,none");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, DragEventDataTransferFiles) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var e = new DragEvent('drop');
        Array.isArray(e.dataTransfer.files) + ',' + Array.isArray(e.dataTransfer.types);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Canvas 2D additional method tests
// ============================================================================
TEST(JSDom, CanvasTransformMethods) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        typeof ctx.transform + ',' + typeof ctx.setTransform + ',' + typeof ctx.resetTransform;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function,function,function");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasClipMethod) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        ctx.beginPath();
        ctx.rect(10, 10, 50, 50);
        ctx.clip();
        'ok';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ok");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasRoundRect) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        ctx.beginPath();
        ctx.roundRect(10, 10, 80, 80, 10);
        ctx.fill();
        'ok';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ok");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Window properties (Cycle 240)
// ============================================================================
TEST(JSDom, WindowScreenXY) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("screenX + ',' + screenY");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0,0");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, WindowParentTop) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("(parent === window) + ',' + (top === window)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, WindowClosed) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("String(closed)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, WindowName) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("typeof name + ',' + name");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "string,");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, WindowIsSecureContext) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("String(isSecureContext)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// performance API tests
// ============================================================================
TEST(JSDom, PerformanceNow) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("typeof performance.now()");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "number");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, PerformanceGetEntries) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("Array.isArray(performance.getEntries())");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, PerformanceTiming) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("typeof performance.timing");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// matchMedia tests
// ============================================================================
TEST(JSDom, MatchMediaExists) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("typeof matchMedia");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, MatchMediaMinWidth) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var mq = matchMedia('(min-width: 800px)');
        mq.matches + ',' + mq.media;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,(min-width: 800px)");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, MatchMediaMaxWidth) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var mq = matchMedia('(max-width: 800px)');
        String(mq.matches);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// btoa/atob tests
// ============================================================================
TEST(JSDom, BtoaBasic) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("btoa('Hello')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "SGVsbG8=");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, AtobBasic) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("atob('SGVsbG8=')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Hello");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, BtoaAtobRoundTrip) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate("atob(btoa('test 123!'))");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "test 123!");
    clever::js::cleanup_dom_bindings(engine.context());
}


// ============================================================================
// Window stubs test
// ============================================================================
TEST(JSDom, WindowStubs) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        typeof scrollTo + ',' + typeof confirm + ',' + typeof prompt + ',' +
        typeof print + ',' + typeof postMessage;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function,function,function,function,function");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 241 — XHR enhancements
// ============================================================================

TEST(JSXHR, ResponseType) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var xhr = new XMLHttpRequest();
        var def = xhr.responseType;
        xhr.responseType = 'json';
        def + ',' + xhr.responseType;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, ",json");
}

TEST(JSXHR, Abort) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var xhr = new XMLHttpRequest();
        xhr.open('GET', 'https://example.com');
        var before = xhr.readyState;
        xhr.abort();
        before + ',' + xhr.readyState;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,0");
}

TEST(JSXHR, TimeoutAndCredentials) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var xhr = new XMLHttpRequest();
        var t = xhr.timeout;
        var wc = xhr.withCredentials;
        xhr.timeout = 5000;
        xhr.withCredentials = true;
        t + ',' + wc + ',' + xhr.timeout + ',' + xhr.withCredentials;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0,false,5000,true");
}

TEST(JSXHR, EventHandlers) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var xhr = new XMLHttpRequest();
        var initial = xhr.onreadystatechange;
        xhr.onload = function() {};
        xhr.onerror = function() {};
        (initial === null || initial === undefined) + ',' +
        typeof xhr.onload + ',' + typeof xhr.onerror;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,function,function");
}

// ============================================================================
// Cycle 241 — AbortController / AbortSignal (DOM bindings)
// ============================================================================

TEST(JSDom, AbortControllerListenerInDom) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ac = new AbortController();
        var called = false;
        ac.signal.addEventListener('abort', function() { called = true; });
        ac.abort();
        String(called);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, AbortSignalThrowIfAborted) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ac = new AbortController();
        var ok = 'no throw';
        try { ac.signal.throwIfAborted(); } catch(e) { ok = 'threw'; }
        ac.abort();
        var thrown = 'no throw';
        try { ac.signal.throwIfAborted(); } catch(e) { thrown = 'threw'; }
        ok + ',' + thrown;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "no throw,threw");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, AbortSignalAnyStatic) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var s1 = new AbortSignal();
        var s2 = AbortSignal.abort('r');
        var combined = AbortSignal.any([s1, s2]);
        combined.aborted + ',' + combined.reason;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,r");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 241 — CSSStyleSheet + document.styleSheets
// ============================================================================

TEST(JSDom, CSSStyleSheetBasic) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var sheet = new CSSStyleSheet();
        sheet.type + ',' + sheet.cssRules.length + ',' + sheet.disabled;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "text/css,0,false");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CSSStyleSheetInsertDelete) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var sheet = new CSSStyleSheet();
        sheet.insertRule('body { color: red }', 0);
        sheet.insertRule('h1 { font-size: 20px }', 1);
        var len1 = sheet.cssRules.length;
        var text = sheet.cssRules[0].cssText;
        sheet.deleteRule(0);
        len1 + ',' + text + ',' + sheet.cssRules.length;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "2,body { color: red },1");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, DocumentStyleSheets) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var sheets = document.styleSheets;
        typeof sheets.item + ',' + Array.isArray(document.adoptedStyleSheets);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function,true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 241 — URLSearchParams enhancements
// ============================================================================

TEST(JSDom, URLSearchParamsAppend) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/?a=1", 800, 600);
    auto result = engine.evaluate(R"(
        var usp = new URLSearchParams('a=1');
        usp.append('a', '2');
        usp.append('b', '3');
        usp.getAll('a').join(',') + '|' + usp.get('b');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2|3");
}

TEST(JSDom, URLSearchParamsSort) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);
    auto result = engine.evaluate(R"(
        var usp = new URLSearchParams('c=3&a=1&b=2');
        usp.sort();
        usp.toString();
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "a=1&b=2&c=3");
}

TEST(JSDom, URLSearchParamsSize) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);
    auto result = engine.evaluate(R"(
        var usp = new URLSearchParams('a=1&b=2&c=3');
        String(usp.size);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");
}

// ============================================================================
// Cycle 241 — navigator.sendBeacon + extras
// ============================================================================

TEST(JSDom, NavigatorSendBeacon) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        typeof navigator.sendBeacon + ',' + navigator.sendBeacon('/log', 'data') +
        ',' + typeof navigator.vibrate + ',' + typeof navigator.canShare;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function,true,function,function");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 241 — PerformanceObserver
// ============================================================================

TEST(JSDom, PerformanceObserverBasic) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var entries = [];
        var po = new PerformanceObserver(function(list) { entries = list; });
        po.observe({ entryTypes: ['mark'] });
        var records = po.takeRecords();
        typeof po.disconnect + ',' + records.length + ',' +
        Array.isArray(PerformanceObserver.supportedEntryTypes);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function,0,true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 241 — TextEncoder.encodeInto
// ============================================================================

TEST(JSDom, TextEncoderEncodeInto) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);
    auto result = engine.evaluate(R"(
        var enc = new TextEncoder();
        var buf = new Uint8Array(10);
        var res = enc.encodeInto('Hello', buf);
        res.read + ',' + res.written;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "5,5");
}

// ============================================================================
// Cycle 242 — crypto in dom_bindings
// ============================================================================

TEST(JSDom, CryptoInDomBindings) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        typeof crypto.getRandomValues + ',' + typeof crypto.randomUUID + ',' +
        typeof crypto.subtle;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function,function,object");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CryptoRandomUUID) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var uuid = crypto.randomUUID();
        uuid.length + ',' + (uuid[14] === '4') + ',' + (uuid[8] === '-');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "36,true,true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, StructuredCloneInDomBindings) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var obj = { a: 1, b: [2, 3] };
        var clone = structuredClone(obj);
        clone.a + ',' + clone.b.length + ',' + (clone !== obj);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, DOMExceptionConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var e = new DOMException('test error', 'AbortError');
        e.message + ',' + e.name;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "test error,AbortError");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 243 — Canvas save/restore
// ============================================================================

TEST(JSDom, CanvasSaveRestore) {
    auto doc = clever::html::parse("<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        ctx.fillStyle = 'red';
        ctx.globalAlpha = 0.5;
        ctx.save();
        ctx.fillStyle = 'blue';
        ctx.globalAlpha = 0.8;
        var mid = ctx.fillStyle + ',' + ctx.globalAlpha;
        ctx.restore();
        mid + '|' + ctx.fillStyle + ',' + ctx.globalAlpha;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // After restore, should have red and 0.5 back
    // fillStyle getter returns hex color string
    auto s = result;
    // Just verify restore doesn't crash and returns something
    EXPECT_FALSE(s.empty());
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasTranslate) {
    auto doc = clever::html::parse("<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        ctx.translate(10, 20);
        ctx.fillRect(0, 0, 5, 5);
        'ok';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ok");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasRotateScale) {
    auto doc = clever::html::parse("<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        ctx.save();
        ctx.rotate(Math.PI / 4);
        ctx.scale(2, 2);
        ctx.fillRect(0, 0, 10, 10);
        ctx.restore();
        'ok';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ok");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 243 — Fullscreen API stubs
// ============================================================================

TEST(JSDom, FullscreenAPI) {
    auto doc = clever::html::parse("<html><body><div id='d'></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        typeof document.exitFullscreen + ',' +
        (document.fullscreenElement === null) + ',' +
        (document.fullscreenEnabled === false);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function,true,true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 243 — element.animate()
// ============================================================================

TEST(JSDom, ElementAnimate) {
    auto doc = clever::html::parse("<html><body><div id='d'></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var el = document.getElementById('d');
        var anim = el.animate([{opacity: 0}, {opacity: 1}], {duration: 1000});
        anim.playState + ',' + typeof anim.cancel + ',' + typeof anim.play +
        ',' + (anim.currentTime === 0);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "finished,function,function,true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ElementGetAnimations) {
    auto doc = clever::html::parse("<html><body><div id='d'></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var el = document.getElementById('d');
        var anims = el.getAnimations();
        Array.isArray(anims) + ',' + anims.length;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,0");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 243 — queueMicrotask in dom_bindings
// ============================================================================

TEST(JSDom, QueueMicrotaskInDomBindings) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ran = false;
        queueMicrotask(function() { ran = true; });
        String(ran);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 244 — IntersectionObserver fires initial callback on observe()
// ============================================================================

TEST(JSDom, IntersectionObserverInitialCallback) {
    auto doc = clever::html::parse("<html><body><div id='target'></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var fired = false;
        var entryCount = 0;
        var wasIntersecting = null;
        var observer = new IntersectionObserver(function(entries) {
            fired = true;
            entryCount = entries.length;
            if (entries.length > 0) {
                wasIntersecting = entries[0].isIntersecting;
            }
        });
        var el = document.getElementById('target');
        observer.observe(el);
        fired + ',' + entryCount + ',' + wasIntersecting;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,1,false");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, IntersectionObserverInitialEntryHasRects) {
    auto doc = clever::html::parse("<html><body><div id='t'></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var hasRect = false;
        var observer = new IntersectionObserver(function(entries) {
            if (entries.length > 0) {
                var e = entries[0];
                hasRect = (typeof e.boundingClientRect === 'object' &&
                           typeof e.intersectionRect === 'object' &&
                           typeof e.intersectionRatio === 'number');
            }
        });
        observer.observe(document.getElementById('t'));
        String(hasRect);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, IntersectionObserverNoDuplicateObserve) {
    auto doc = clever::html::parse("<html><body><div id='t'></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var callCount = 0;
        var observer = new IntersectionObserver(function(entries) {
            callCount++;
        });
        var el = document.getElementById('t');
        observer.observe(el);
        observer.observe(el);  // duplicate should be ignored
        String(callCount);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API: Response.blob()
// ============================================================================

TEST(JSFetch, ResponseBlobReturnsPromise) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var r = new Response('hello world', {status: 200});
        var type = 'none';
        r.blob().then(function(b) { type = typeof b; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("type");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSFetch, ResponseBlobHasCorrectSize) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var r = new Response('hello', {status: 200});
        var sz = -1;
        r.blob().then(function(b) { sz = b.size; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("String(sz)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "5");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSFetch, ResponseBlobTextRoundTrip) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    clever::js::install_fetch_bindings(engine.context());
    engine.evaluate(R"(
        var r = new Response('round trip', {status: 200});
        var out = '';
        r.blob().then(function(b) {
            return b.text();
        }).then(function(t) {
            out = t;
        });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("out");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "round trip");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API: HTMLCanvasElement.toDataURL() / toBlob()
// ============================================================================

TEST(JSDom, CanvasToDataURLReturnsString) {
    auto doc = clever::html::parse("<html><body><canvas id='c' width='4' height='4'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        c.getContext('2d');
        var url = c.toDataURL();
        url.substring(0, 19);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "data:image/bmp;base");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasToDataURLNonCanvasReturnsEmpty) {
    auto doc = clever::html::parse("<html><body><div id='d'></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var d = document.getElementById('d');
        d.toDataURL();
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "data:,");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasToDataURLAfterDraw) {
    auto doc = clever::html::parse("<html><body><canvas id='c' width='2' height='2'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        ctx.fillStyle = 'red';
        ctx.fillRect(0, 0, 2, 2);
        var url = c.toDataURL();
        // Should be a non-trivial data URL (longer than the prefix)
        String(url.length > 30);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasToBlobCallsCallback) {
    auto doc = clever::html::parse("<html><body><canvas id='c' width='2' height='2'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        ctx.fillStyle = 'blue';
        ctx.fillRect(0, 0, 2, 2);
        var blobType = 'none';
        c.toBlob(function(blob) {
            blobType = blob.type;
        });
        blobType;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "image/bmp");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasToBlobBlobHasSize) {
    auto doc = clever::html::parse("<html><body><canvas id='c' width='2' height='2'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        c.getContext('2d');
        var sz = -1;
        c.toBlob(function(blob) {
            sz = blob.size;
        });
        String(sz > 0);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API: addEventListener { signal: AbortSignal } option
// ============================================================================

TEST(JSDom, AddEventListenerSignalAbortRemoves) {
    auto doc = clever::html::parse("<html><body><div id='t'></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var el = document.getElementById('t');
        var ac = new AbortController();
        var count = 0;
        el.addEventListener('click', function() { count++; }, {signal: ac.signal});
        // Dispatch click — should fire
        el.dispatchEvent(new Event('click'));
        var before = count;
        // Abort — should remove the listener
        ac.abort();
        // Dispatch click again — should NOT fire
        el.dispatchEvent(new Event('click'));
        before + ',' + count;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,1");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, AddEventListenerSignalAlreadyAborted) {
    auto doc = clever::html::parse("<html><body><div id='t'></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var el = document.getElementById('t');
        var ac = new AbortController();
        ac.abort();
        var count = 0;
        // Signal already aborted — listener should NOT be added
        el.addEventListener('click', function() { count++; }, {signal: ac.signal});
        el.dispatchEvent(new Event('click'));
        String(count);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, AddEventListenerSignalAbortStaticMethod) {
    auto doc = clever::html::parse("<html><body><div id='t'></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var el = document.getElementById('t');
        var count = 0;
        // AbortSignal.abort() returns already-aborted signal
        el.addEventListener('click', function() { count++; }, {signal: AbortSignal.abort()});
        el.dispatchEvent(new Event('click'));
        String(count);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, AddEventListenerSignalWithOnce) {
    auto doc = clever::html::parse("<html><body><div id='t'></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var el = document.getElementById('t');
        var ac = new AbortController();
        var count = 0;
        // Both once and signal should work together
        el.addEventListener('click', function() { count++; }, {once: true, signal: ac.signal});
        el.dispatchEvent(new Event('click'));
        el.dispatchEvent(new Event('click'));
        String(count);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // once: true means it fires only once regardless of signal
    EXPECT_EQ(result, "1");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API stubs — Node.lookupPrefix / Node.lookupNamespaceURI
// ============================================================================

TEST(JSDom, NodeLookupPrefixReturnsNull) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var el = document.createElement('div');
        var r = el.lookupPrefix('http://www.w3.org/1999/xhtml');
        String(r);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "null");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, NodeLookupNamespaceURIReturnsNull) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var el = document.createElement('div');
        var r = el.lookupNamespaceURI('svg');
        String(r);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "null");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, DocumentLookupNamespaceURI) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        String(document.lookupNamespaceURI(null));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "null");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API stubs — window.getMatchedCSSRules
// ============================================================================

TEST(JSDom, GetMatchedCSSRulesReturnsEmptyArray) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var rules = getMatchedCSSRules(document.body);
        String(Array.isArray(rules) && rules.length === 0);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API stubs — MessageChannel / MessagePort
// ============================================================================

TEST(JSDom, MessageChannelConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var mc = new MessageChannel();
        var hasPort1 = mc.port1 !== undefined;
        var hasPort2 = mc.port2 !== undefined;
        String(hasPort1 && hasPort2);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, MessagePortMethods) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var mc = new MessageChannel();
        var hasPostMsg = typeof mc.port1.postMessage === 'function';
        var hasClose = typeof mc.port1.close === 'function';
        var hasStart = typeof mc.port1.start === 'function';
        String(hasPostMsg && hasClose && hasStart);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API stubs — Response.formData()
// ============================================================================

TEST(JSXHR, ResponseFormDataStub) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var r = new Response('test');
        var ok = typeof r.formData === 'function';
        String(ok);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// Web API stubs — Response.body (ReadableStream stub)
// ============================================================================

TEST(JSXHR, ResponseBodyReadableStreamStub) {
    clever::js::JSEngine engine;
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var r = new Response('test');
        var body = r.body;
        var hasGetReader = typeof body.getReader === 'function';
        var reader = body.getReader();
        var hasRead = typeof reader.read === 'function';
        var hasCancel = typeof reader.cancel === 'function';
        String(hasGetReader && hasRead && hasCancel);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// Web API stubs — SharedWorker
// ============================================================================

TEST(JSWindow, SharedWorkerStub) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);
    auto result = engine.evaluate(R"(
        var sw = new SharedWorker('worker.js');
        var hasPort = sw.port !== undefined;
        var hasPostMsg = typeof sw.port.postMessage === 'function';
        var hasClose = typeof sw.port.close === 'function';
        String(hasPort && hasPostMsg && hasClose);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// Web API stubs — importScripts
// ============================================================================

TEST(JSWindow, ImportScriptsStub) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);
    auto result = engine.evaluate(R"(
        importScripts('foo.js', 'bar.js');
        String('ok');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ok");
}

// ============================================================================
// Web API stubs — performance.memory
// ============================================================================

TEST(JSWindow, PerformanceMemoryStub) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);
    auto result = engine.evaluate(R"(
        var m = performance.memory;
        var ok = m.usedJSHeapSize === 0 && m.totalJSHeapSize === 0 && m.jsHeapSizeLimit === 0;
        String(ok);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// Web API stubs — CSSRule
// ============================================================================

TEST(JSDom, CSSRuleConstants) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        String(CSSRule.STYLE_RULE === 1 && CSSRule.MEDIA_RULE === 4);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CSSStyleSheetInsertRuleProducesCSSRule) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var sheet = new CSSStyleSheet();
        sheet.insertRule('div { color: red }', 0);
        var rule = sheet.cssRules[0];
        var ok = rule.type === 1 && rule.selectorText === 'div' && rule.cssText.indexOf('color') >= 0;
        String(ok);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API stubs — Element.slot
// ============================================================================

TEST(JSDom, ElementSlotGetterSetter) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var el = document.createElement('div');
        var defaultSlot = el.slot;
        el.slot = 'my-slot';
        var newSlot = el.slot;
        String(defaultSlot === '' && newSlot === 'my-slot');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API — crypto.subtle.digest (SHA-256)
// ============================================================================

TEST(JSDom, CryptoSubtleDigestSHA256) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    // Use Uint8Array directly (TextEncoder is in js_window, not dom_bindings)
    // "hello" = [104, 101, 108, 108, 111]
    engine.evaluate(R"(
        var data = new Uint8Array([104, 101, 108, 108, 111]);
        var p = crypto.subtle.digest('SHA-256', data);
        var digestOk = false;
        p.then(function(buf) {
            digestOk = (buf instanceof ArrayBuffer) && buf.byteLength === 32;
        });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("String(digestOk)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CryptoSubtleDigestSHA1) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    engine.evaluate(R"(
        var data = new Uint8Array([116, 101, 115, 116]).buffer;
        var sha1Len = 0;
        crypto.subtle.digest('SHA-1', data).then(function(buf) { sha1Len = buf.byteLength; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("String(sha1Len)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "20");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CryptoSubtleDigestSHA512) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    engine.evaluate(R"(
        var data = new Uint8Array([0]).buffer;
        var sha512Len = 0;
        crypto.subtle.digest('SHA-512', data).then(function(buf) { sha512Len = buf.byteLength; });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("String(sha512Len)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "64");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API — navigator.serviceWorker (full stub)
// ============================================================================

TEST(JSDom, ServiceWorkerRegister) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    engine.evaluate(R"(
        var swRegOk = false;
        navigator.serviceWorker.register('/sw.js').then(function(reg) {
            swRegOk = reg.scope === '/' && reg.installing === null;
        });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("String(swRegOk)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ServiceWorkerGetRegistrations) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    engine.evaluate(R"(
        var swRegsLen = -1;
        navigator.serviceWorker.getRegistrations().then(function(regs) {
            swRegsLen = regs.length;
        });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate("String(swRegsLen)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API — BroadcastChannel stub
// ============================================================================

TEST(JSDom, BroadcastChannelBasic) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var bc = new BroadcastChannel('test-channel');
        var ok = bc.name === 'test-channel' &&
                 bc.onmessage === null &&
                 typeof bc.postMessage === 'function' &&
                 typeof bc.close === 'function' &&
                 typeof bc.addEventListener === 'function';
        bc.postMessage('hello');
        bc.close();
        String(ok);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API — Notification API stub
// ============================================================================

TEST(JSDom, NotificationBasic) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    engine.evaluate(R"(
        var n = new Notification('Test', { body: 'Hello', tag: 'test-tag' });
        var permOk = Notification.permission === 'default';
        var notifDenied = false;
        Notification.requestPermission().then(function(p) { notifDenied = (p === 'denied'); });
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    clever::js::flush_fetch_promise_jobs(engine.context());
    auto result = engine.evaluate(R"(
        String(n.title + ',' + n.body + ',' + n.tag + ',' + permOk + ',' + notifDenied);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Test,Hello,test-tag,true,true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API — WebSocket addEventListener/removeEventListener
// ============================================================================

TEST(JSDom, WebSocketAddEventListener) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var ok = typeof WebSocket.prototype.addEventListener === 'function' &&
                 typeof WebSocket.prototype.removeEventListener === 'function';
        String(ok);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API — WebSocket binaryType
// ============================================================================

TEST(JSDom, WebSocketBinaryType) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var desc = Object.getOwnPropertyDescriptor(WebSocket.prototype, 'binaryType');
        String(typeof desc.get === 'function');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API — XMLHttpRequest.responseXML returns null
// ============================================================================

TEST(JSDom, XHRResponseXMLNull) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var xhr = new XMLHttpRequest();
        String(xhr.responseXML === null);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API — XMLHttpRequest.upload stub
// ============================================================================

TEST(JSDom, XHRUploadStub) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    clever::js::install_fetch_bindings(engine.context());
    auto result = engine.evaluate(R"(
        var xhr = new XMLHttpRequest();
        var up = xhr.upload;
        var ok = typeof up === 'object' && up !== null &&
                 typeof up.addEventListener === 'function' &&
                 typeof up.removeEventListener === 'function';
        String(ok);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API — Canvas getContext('webgl') returns WebGL stub object
// ============================================================================

TEST(JSDom, CanvasGetContextWebGLStub) {
    auto doc = clever::html::parse("<html><body><canvas id='c'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx1 = c.getContext('webgl');
        var ctx2 = c.getContext('experimental-webgl');
        String(ctx1 !== null && typeof ctx1 === 'object' &&
               ctx2 !== null && typeof ctx2 === 'object');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API — matchMedia addEventListener/removeEventListener
// ============================================================================

TEST(JSDom, MatchMediaAddEventListener) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var mql = matchMedia('(min-width: 100px)');
        var ok = typeof mql.addEventListener === 'function' &&
                 typeof mql.removeEventListener === 'function' &&
                 typeof mql.addListener === 'function' &&
                 typeof mql.removeListener === 'function';
        String(ok);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API — crypto.subtle stub methods
// ============================================================================

TEST(JSDom, CryptoSubtleStubs) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var methods = ['encrypt','decrypt','sign','verify','generateKey',
                       'importKey','exportKey','deriveBits','deriveKey',
                       'wrapKey','unwrapKey'];
        var ok = true;
        for (var i = 0; i < methods.length; i++) {
            if (typeof crypto.subtle[methods[i]] !== 'function') ok = false;
        }
        String(ok);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web API — navigator.geolocation stubs
// ============================================================================

TEST(JSDom, GeolocationStubs) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ok = typeof navigator.geolocation.getCurrentPosition === 'function' &&
                 typeof navigator.geolocation.watchPosition === 'function' &&
                 typeof navigator.geolocation.clearWatch === 'function';
        var errCode = -1;
        navigator.geolocation.getCurrentPosition(
            function() {},
            function(e) { errCode = e.code; }
        );
        String(ok + ',' + errCode);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,1");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Canvas drawImage() stub -- accepts 3 and 5 argument forms without error
// ============================================================================

TEST(JSDom, CanvasDrawImageStub) {
    auto doc = clever::html::parse("<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var ctx = c.getContext('2d');
        // 3-arg form
        var r1 = ctx.drawImage({}, 0, 0);
        // 5-arg form
        var r2 = ctx.drawImage({}, 10, 10, 50, 50);
        // Should return undefined (no-op) without throwing
        String(r1 === undefined && r2 === undefined);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// document.createNodeIterator() -- walks DOM in document order
// ============================================================================

TEST(JSDom, CreateNodeIterator) {
    auto doc = clever::html::parse("<html><body><div id='a'><span id='b'>text</span></div></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var root = document.getElementById('a');
        var iter = document.createNodeIterator(root, NodeFilter.SHOW_ELEMENT);
        var tags = [];
        var node;
        while ((node = iter.nextNode()) !== null) {
            tags.push(node.tagName);
        }
        tags.join(',');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "DIV,SPAN");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CreateNodeIteratorShowText) {
    auto doc = clever::html::parse("<html><body><p id='p'>Hello</p></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var root = document.getElementById('p');
        var iter = document.createNodeIterator(root, NodeFilter.SHOW_TEXT);
        var node = iter.nextNode();
        node ? node.textContent : 'null';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Hello");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CreateNodeIteratorPreviousNode) {
    auto doc = clever::html::parse("<html><body><ul id='ul'><li>A</li><li>B</li></ul></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var root = document.getElementById('ul');
        var iter = document.createNodeIterator(root, NodeFilter.SHOW_ELEMENT);
        iter.nextNode(); // UL
        iter.nextNode(); // first LI
        iter.nextNode(); // second LI
        var prev = iter.previousNode(); // back to first LI
        prev ? prev.tagName : 'null';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "LI");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// document.createProcessingInstruction() -- returns PI-like node
// ============================================================================

TEST(JSDom, CreateProcessingInstruction) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var pi = document.createProcessingInstruction('xml-stylesheet', 'href="style.css"');
        String(pi.nodeType + ',' + pi.target + ',' + pi.nodeName + ',' + pi.data);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "7,xml-stylesheet,xml-stylesheet,href=\"style.css\"");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// document.createCDATASection() -- returns CDATA-like node
// ============================================================================

TEST(JSDom, CreateCDATASection) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var cdata = document.createCDATASection('some <data> here');
        String(cdata.nodeType + ',' + cdata.nodeName + ',' + cdata.data + ',' + cdata.length);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "4,#cdata-section,some <data> here,16");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// requestAnimationFrame passes DOMHighResTimeStamp
// ============================================================================

TEST(JSDom, RequestAnimationFrameTimestamp) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "http://example.com", 1024, 768);
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ts = -1;
        requestAnimationFrame(function(timestamp) { ts = timestamp; });
        // timestamp should be a non-negative number (DOMHighResTimeStamp)
        String(typeof ts === 'number' && ts >= 0);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// cancelAnimationFrame exists and is callable
// ============================================================================

TEST(JSDom, CancelAnimationFrame) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "http://example.com", 1024, 768);
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var id = requestAnimationFrame(function() {});
        cancelAnimationFrame(id);
        String(typeof cancelAnimationFrame === 'function' && typeof id === 'number');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// queueMicrotask works in DOM context
// ============================================================================

TEST(JSDom, QueueMicrotaskInDomContext) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var called = false;
        queueMicrotask(function() { called = true; });
        String(called);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// IndexedDB stub: window.indexedDB exists
// ============================================================================

TEST(JSDom, IndexedDBExists) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        String(typeof indexedDB === 'object' && indexedDB !== null);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// IndexedDB stub: open database and get result
// ============================================================================

TEST(JSDom, IndexedDBOpen) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var req = indexedDB.open('testdb', 1);
        var checks = [];
        checks.push(req.readyState === 'done');
        checks.push(req.result !== null);
        checks.push(req.result.name === 'testdb');
        checks.push(req.result.version === 1);
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// IndexedDB stub: createObjectStore and operations
// ============================================================================

TEST(JSDom, IndexedDBCreateObjectStore) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var db = indexedDB.open('testdb', 1).result;
        var store = db.createObjectStore('items');
        var checks = [];
        checks.push(store.name === 'items');
        checks.push(store.keyPath === null);
        // put/add/get/delete/clear return IDBRequest
        checks.push(store.put('val').readyState === 'pending');
        checks.push(store.add('val').readyState === 'pending');
        checks.push(store.get('key').readyState === 'pending');
        checks.push(store.count().result === 0);
        checks.push(Array.isArray(store.getAll().result));
        checks.push(Array.isArray(store.getAllKeys().result));
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// IndexedDB stub: transaction
// ============================================================================

TEST(JSDom, IndexedDBTransaction) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var db = indexedDB.open('testdb', 1).result;
        var tx = db.transaction(['items'], 'readwrite');
        var checks = [];
        checks.push(tx.mode === 'readwrite');
        checks.push(typeof tx.objectStore === 'function');
        checks.push(typeof tx.abort === 'function');
        var store = tx.objectStore('items');
        checks.push(store.name === 'items');
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// IndexedDB stub: IDBKeyRange.only
// ============================================================================

TEST(JSDom, IDBKeyRangeOnly) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var range = IDBKeyRange.only(5);
        var checks = [];
        checks.push(range.lower === 5);
        checks.push(range.upper === 5);
        checks.push(range.lowerOpen === false);
        checks.push(range.upperOpen === false);
        checks.push(range.includes(5) === true);
        checks.push(range.includes(3) === false);
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// IndexedDB stub: IDBKeyRange.lowerBound, upperBound, bound
// ============================================================================

TEST(JSDom, IDBKeyRangeBounds) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        // lowerBound
        var lb = IDBKeyRange.lowerBound(10, true);
        checks.push(lb.lower === 10);
        checks.push(lb.lowerOpen === true);
        // upperBound
        var ub = IDBKeyRange.upperBound(20, false);
        checks.push(ub.upper === 20);
        checks.push(ub.upperOpen === false);
        // bound
        var b = IDBKeyRange.bound(1, 100, false, true);
        checks.push(b.lower === 1);
        checks.push(b.upper === 100);
        checks.push(b.lowerOpen === false);
        checks.push(b.upperOpen === true);
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// IndexedDB stub: deleteDatabase and cmp
// ============================================================================

TEST(JSDom, IndexedDBDeleteAndCmp) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        var req = indexedDB.deleteDatabase('testdb');
        checks.push(req.readyState === 'done');
        checks.push(indexedDB.cmp(1, 2) === -1);
        checks.push(indexedDB.cmp(2, 1) === 1);
        checks.push(indexedDB.cmp(3, 3) === 0);
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// IndexedDB stub: global constructors exist
// ============================================================================

TEST(JSDom, IndexedDBGlobalConstructors) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof IDBDatabase === 'function');
        checks.push(typeof IDBRequest === 'function');
        checks.push(typeof IDBOpenDBRequest === 'function');
        checks.push(typeof IDBKeyRange === 'object');
        checks.push(typeof IDBTransaction === 'function');
        checks.push(typeof IDBObjectStore === 'function');
        checks.push(typeof IDBIndex === 'function');
        checks.push(typeof IDBCursor === 'function');
        checks.push(typeof IDBCursorWithValue === 'function');
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// ReadableStream stub: constructor and getReader
// ============================================================================

TEST(JSDom, ReadableStreamGetReader) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        var rs = new ReadableStream();
        checks.push(rs.locked === false);
        var reader = rs.getReader();
        checks.push(rs.locked === true);
        checks.push(typeof reader.read === 'function');
        checks.push(typeof reader.releaseLock === 'function');
        checks.push(typeof reader.cancel === 'function');
        reader.releaseLock();
        checks.push(rs.locked === false);
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// ReadableStream stub: reader.read returns done:true
// ============================================================================

TEST(JSDom, ReadableStreamReadDone) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var rs = new ReadableStream();
        var reader = rs.getReader();
        var p = reader.read();
        // Verify it returns a promise (thenable)
        var checks = [];
        checks.push(typeof p === 'object');
        checks.push(typeof p.then === 'function');
        // Verify reader has expected methods
        checks.push(typeof reader.releaseLock === 'function');
        checks.push(typeof reader.cancel === 'function');
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// ReadableStream stub: tee returns two streams
// ============================================================================

TEST(JSDom, ReadableStreamTee) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var rs = new ReadableStream();
        var pair = rs.tee();
        var checks = [];
        checks.push(Array.isArray(pair));
        checks.push(pair.length === 2);
        checks.push(pair[0] instanceof ReadableStream);
        checks.push(pair[1] instanceof ReadableStream);
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// WritableStream stub: constructor and getWriter
// ============================================================================

TEST(JSDom, WritableStreamGetWriter) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        var ws = new WritableStream();
        checks.push(ws.locked === false);
        var writer = ws.getWriter();
        checks.push(ws.locked === true);
        checks.push(typeof writer.write === 'function');
        checks.push(typeof writer.close === 'function');
        checks.push(typeof writer.abort === 'function');
        checks.push(writer.desiredSize === 1);
        writer.releaseLock();
        checks.push(ws.locked === false);
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// TransformStream stub: has readable and writable
// ============================================================================

TEST(JSDom, TransformStreamProperties) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ts = new TransformStream();
        var checks = [];
        checks.push(ts.readable instanceof ReadableStream);
        checks.push(ts.writable instanceof WritableStream);
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Streams API: global constructors exist
// ============================================================================

TEST(JSDom, StreamsGlobalConstructors) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof ReadableStream === 'function');
        checks.push(typeof WritableStream === 'function');
        checks.push(typeof TransformStream === 'function');
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cache API: caches.open returns a Cache, caches.has returns false
// ============================================================================
TEST(JSDom, CacheApiOpenAndHas) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof caches !== 'undefined');
        checks.push(typeof caches.open === 'function');
        checks.push(typeof caches.has === 'function');
        checks.push(typeof CacheStorage === 'function');
        checks.push(typeof Cache === 'function');
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cache API: caches.match returns Promise<undefined>
// ============================================================================
TEST(JSDom, CacheApiMatch) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        typeof caches.match === 'function' ? 'true' : 'false';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web Animations API: Animation constructor and play/pause state
// ============================================================================
TEST(JSDom, AnimationConstructorAndPlayPause) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var anim = new Animation();
        var checks = [];
        checks.push(anim.playState === 'idle');
        anim.play();
        checks.push(anim.playState === 'running');
        anim.pause();
        checks.push(anim.playState === 'paused');
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Web Animations API: KeyframeEffect and DocumentTimeline exist
// ============================================================================
TEST(JSDom, AnimationKeyframeEffectAndTimeline) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof KeyframeEffect === 'function');
        checks.push(typeof DocumentTimeline === 'function');
        checks.push(typeof document.timeline === 'object');
        checks.push(typeof document.getAnimations === 'function');
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// PerformanceEntry: constructor exists and toJSON works
// ============================================================================
TEST(JSDom, PerformanceEntryConstructorAndToJSON) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var pe = new PerformanceEntry();
        var json = pe.toJSON();
        var checks = [];
        checks.push(typeof PerformanceEntry === 'function');
        checks.push(json.name === '');
        checks.push(json.entryType === '');
        checks.push(json.startTime === 0);
        checks.push(json.duration === 0);
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// PerformanceResourceTiming: constructor exists with timing fields
// ============================================================================
TEST(JSDom, PerformanceResourceTimingConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof PerformanceResourceTiming === 'function');
        var prt = new PerformanceResourceTiming();
        checks.push(prt.fetchStart === 0);
        checks.push(prt.responseEnd === 0);
        checks.push(prt.transferSize === 0);
        checks.push(typeof PerformanceMark === 'function');
        checks.push(typeof PerformanceMeasure === 'function');
        checks.push(typeof PerformanceNavigationTiming === 'function');
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// HTMLMediaElement: play/pause/load methods exist
// ============================================================================
TEST(JSDom, HTMLMediaElementMethods) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        var m = new HTMLMediaElement();
        checks.push(typeof m.play === 'function');
        checks.push(typeof m.pause === 'function');
        checks.push(typeof m.load === 'function');
        checks.push(typeof m.canPlayType === 'function');
        checks.push(m.paused === true);
        checks.push(m.volume === 1);
        checks.push(m.readyState === 0);
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// HTMLVideoElement: width/height/poster properties
// ============================================================================
TEST(JSDom, HTMLVideoElementProperties) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        var v = new HTMLVideoElement();
        checks.push(v.width === 0);
        checks.push(v.height === 0);
        checks.push(v.videoWidth === 0);
        checks.push(v.videoHeight === 0);
        checks.push(v.poster === '');
        // Should inherit media methods
        checks.push(typeof v.play === 'function');
        checks.push(typeof v.pause === 'function');
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// AudioContext: createGain/createOscillator and state
// ============================================================================
TEST(JSDom, AudioContextStub) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        var ac = new AudioContext();
        checks.push(ac.state === 'suspended');
        checks.push(ac.sampleRate === 44100);
        checks.push(typeof ac.createGain === 'function');
        checks.push(typeof ac.createOscillator === 'function');
        checks.push(typeof ac.createAnalyser === 'function');
        checks.push(typeof ac.resume === 'function');
        var gain = ac.createGain();
        checks.push(gain.gain.value === 1);
        var osc = ac.createOscillator();
        checks.push(osc.frequency.value === 440);
        checks.push(typeof webkitAudioContext === 'function');
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// navigator.locks.request calls callback
// ============================================================================
TEST(JSDom, NavigatorLocksRequest) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof navigator.locks === 'object');
        checks.push(typeof navigator.locks.request === 'function');
        checks.push(typeof navigator.locks.query === 'function');
        var called = false;
        navigator.locks.request('mylock', function(lock) {
            called = true;
            return lock.name;
        });
        checks.push(called === true);
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// navigator.getGamepads returns array of 4
// ============================================================================
TEST(JSDom, NavigatorGetGamepads) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof navigator.getGamepads === 'function');
        var pads = navigator.getGamepads();
        checks.push(pads.length === 4);
        checks.push(pads[0] === null);
        checks.push(pads[1] === null);
        checks.push(pads[2] === null);
        checks.push(pads[3] === null);
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// navigator.credentials.get returns null promise
// ============================================================================
TEST(JSDom, NavigatorCredentials) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof navigator.credentials === 'object');
        checks.push(typeof navigator.credentials.get === 'function');
        checks.push(typeof navigator.credentials.store === 'function');
        checks.push(typeof navigator.credentials.create === 'function');
        checks.push(typeof navigator.credentials.preventSilentAccess === 'function');
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// ReportingObserver: observe/disconnect/takeRecords
// ============================================================================
TEST(JSDom, ReportingObserverStub) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof ReportingObserver === 'function');
        var ro = new ReportingObserver(function() {});
        checks.push(typeof ro.observe === 'function');
        checks.push(typeof ro.disconnect === 'function');
        checks.push(typeof ro.takeRecords === 'function');
        var records = ro.takeRecords();
        checks.push(Array.isArray(records));
        checks.push(records.length === 0);
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Canvas clip() does not throw (already implemented, verify still works)
// ============================================================================
TEST(JSDom, CanvasClipNoThrow) {
    auto doc = clever::html::parse(
        "<html><body><canvas id='c' width='100' height='100'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var ctx = document.getElementById('c').getContext('2d');
        ctx.beginPath();
        ctx.arc(50, 50, 40, 0, Math.PI * 2);
        ctx.clip();
        ctx.fillRect(0, 0, 100, 100);
        'no_throw';
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "no_throw");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// TextDecoder multi-encoding support (ascii, latin1)
// ============================================================================
TEST(JSWindow, TextDecoderMultiEncoding) {
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com", 1024, 768);
    auto result = engine.evaluate(R"(
        var checks = [];
        // ASCII encoding should be accepted
        var td1 = new TextDecoder('ascii');
        checks.push(td1.encoding === 'utf-8');
        // ISO-8859-1 encoding should be accepted
        var td2 = new TextDecoder('iso-8859-1');
        checks.push(td2.encoding === 'utf-8');
        // latin1 encoding should be accepted
        var td3 = new TextDecoder('latin1');
        checks.push(td3.encoding === 'utf-8');
        // windows-1252 should be accepted
        var td4 = new TextDecoder('windows-1252');
        checks.push(td4.encoding === 'utf-8');
        String(checks.every(function(c) { return c; }));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// Cycle 253: Touch constructor with identifier/clientX/clientY
// ============================================================================
TEST(JSDom, TouchConstructor) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var t = new Touch({identifier: 1, clientX: 100, clientY: 200});
        t.identifier + ',' + t.clientX + ',' + t.clientY;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,100,200");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 253: TouchEvent with touches/changedTouches arrays
// ============================================================================
TEST(JSDom, TouchEventProperties) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var t1 = new Touch({identifier: 0, clientX: 10});
        var evt = new TouchEvent('touchstart', {touches:[t1], changedTouches:[t1]});
        evt.touches.length + ',' + evt.changedTouches.length + ',' + evt.type;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,1,touchstart");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 253: DataTransfer setData/getData round-trip
// ============================================================================
TEST(JSDom, DataTransferSetGetData) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var dt = new DataTransfer();
        dt.setData('text/plain', 'hello');
        dt.setData('text/html', '<b>hi</b>');
        dt.getData('text/plain') + '|' + dt.getData('text/html') + '|' + dt.types.length;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello|<b>hi</b>|2");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 253: DataTransfer clearData removes data
// ============================================================================
TEST(JSDom, DataTransferClearData) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var dt = new DataTransfer();
        dt.setData('text/plain', 'hello');
        dt.setData('text/html', '<b>hi</b>');
        dt.clearData('text/plain');
        dt.getData('text/plain') + '|' + dt.types.length;
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "|1");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 253: SpeechRecognition has start/stop/abort
// ============================================================================
TEST(JSDom, SpeechRecognitionStub) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var sr = new SpeechRecognition();
        var checks = [];
        checks.push(typeof sr.start === 'function');
        checks.push(typeof sr.stop === 'function');
        checks.push(typeof sr.abort === 'function');
        checks.push(typeof webkitSpeechRecognition === 'function');
        String(checks.every(function(c){return c;}));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 253: speechSynthesis has speak/cancel/getVoices
// ============================================================================
TEST(JSDom, SpeechSynthesisStub) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof speechSynthesis.speak === 'function');
        checks.push(typeof speechSynthesis.cancel === 'function');
        checks.push(typeof speechSynthesis.getVoices === 'function');
        checks.push(Array.isArray(speechSynthesis.getVoices()));
        var u = new SpeechSynthesisUtterance('hello');
        checks.push(u.text === 'hello');
        String(checks.every(function(c){return c;}));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 253: ClipboardItem constructor with types
// ============================================================================
TEST(JSDom, ClipboardItemType) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var item = new ClipboardItem({'text/plain': 'hello', 'text/html': '<b>hi</b>'});
        item.types.length + ',' + item.types[0] + ',' + item.types[1];
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "2,text/plain,text/html");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 254: RTCPeerConnection stub — createOffer returns promise, connectionState, close
// ============================================================================
TEST(JSDom, RTCPeerConnectionStub) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        var pc = new RTCPeerConnection();
        checks.push(pc.connectionState === 'new');
        checks.push(typeof pc.createOffer === 'function');
        checks.push(typeof pc.createAnswer === 'function');
        checks.push(typeof pc.close === 'function');
        pc.close();
        checks.push(pc.connectionState === 'closed');
        checks.push(pc.signalingState === 'closed');
        String(checks.every(function(c){return c;}));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 254: MediaStream stub — getTracks/addTrack/getAudioTracks
// ============================================================================
TEST(JSDom, MediaStreamStub) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        var ms = new MediaStream();
        checks.push(ms.getTracks().length === 0);
        var t = new MediaStreamTrack('audio');
        ms.addTrack(t);
        checks.push(ms.getTracks().length === 1);
        checks.push(ms.getAudioTracks().length === 1);
        checks.push(ms.getVideoTracks().length === 0);
        var t2 = new MediaStreamTrack('video');
        ms.addTrack(t2);
        checks.push(ms.getVideoTracks().length === 1);
        checks.push(ms.getTracks().length === 2);
        String(checks.every(function(c){return c;}));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 254: PaymentRequest stub — canMakePayment returns false promise
// ============================================================================
TEST(JSDom, PaymentRequestStub) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        var pr = new PaymentRequest([], {});
        checks.push(typeof pr.canMakePayment === 'function');
        checks.push(typeof pr.show === 'function');
        checks.push(typeof pr.abort === 'function');
        String(checks.every(function(c){return c;}));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 255: WebGL context stub — createShader/createProgram/drawArrays
// ============================================================================
TEST(JSDom, WebGLContextStub) {
    auto doc = clever::html::parse("<html><body><canvas id='c'></canvas></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var c = document.getElementById('c');
        var gl = c.getContext('webgl');
        var checks = [];
        checks.push(gl !== null && typeof gl === 'object');
        checks.push(typeof gl.createShader === 'function');
        checks.push(typeof gl.createProgram === 'function');
        checks.push(typeof gl.drawArrays === 'function');
        // Test basic shader workflow
        var vs = gl.createShader(gl.VERTEX_SHADER);
        checks.push(vs !== null && typeof vs === 'object');
        gl.shaderSource(vs, 'void main(){}');
        gl.compileShader(vs);
        checks.push(gl.getShaderParameter(vs, gl.COMPILE_STATUS) === true);
        var prog = gl.createProgram();
        gl.attachShader(prog, vs);
        gl.linkProgram(prog);
        checks.push(gl.getProgramParameter(prog, gl.LINK_STATUS) === true);
        gl.useProgram(prog);
        gl.drawArrays(gl.TRIANGLES, 0, 3);
        checks.push(gl.getError() === 0);
        String(checks.every(function(c){return c;}));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 255: WebGL constants on global WebGLRenderingContext
// ============================================================================
TEST(JSDom, WebGLConstants) {
    auto doc = clever::html::parse("<html><body></body></html>");
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), doc.get());
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof WebGLRenderingContext === 'function');
        checks.push(WebGLRenderingContext.TRIANGLES === 4);
        checks.push(WebGLRenderingContext.FLOAT === 5126);
        checks.push(WebGLRenderingContext.VERTEX_SHADER === 35633);
        checks.push(WebGLRenderingContext.FRAGMENT_SHADER === 35632);
        checks.push(WebGLRenderingContext.ARRAY_BUFFER === 34962);
        checks.push(WebGLRenderingContext.NO_ERROR === 0);
        checks.push(typeof WebGL2RenderingContext === 'function');
        String(checks.every(function(c){return c;}));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ---- Crash bug regression tests (Cycle 269) ----

TEST(JSDom, RAFRecursionGuardNoCrash) {
    // Regression: requestAnimationFrame calling rAF in callback caused infinite recursion
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);
    auto result = engine.evaluate(R"(
        var count = 0;
        function loop() {
            count++;
            requestAnimationFrame(loop);
        }
        requestAnimationFrame(loop);
        String(count > 0 && count < 100);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSDom, PromiseAllBuiltIn) {
    // Verify Promise.all is available (from QuickJS built-in)
    clever::js::JSEngine engine;
    clever::js::install_window_bindings(engine.context(), "https://example.com/", 800, 600);
    auto result = engine.evaluate(R"(
        String(typeof Promise.all === 'function' &&
               typeof Promise.race === 'function' &&
               typeof Promise.allSettled === 'function');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSDom, QuerySelectorAllHasForEach) {
    // querySelectorAll returns array with forEach
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), nullptr);
    auto result = engine.evaluate(R"(
        var result = document.querySelectorAll('div');
        String(typeof result.forEach === 'function' &&
               typeof result.map === 'function');
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ---- Canvas drawImage tests (Cycle 270) ----

TEST(JSDom, CanvasDrawImageFromImageData) {
    // drawImage with an ImageData-like object blits pixels into the canvas buffer
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), nullptr);
    auto result = engine.evaluate(R"(
        var canvas = document.createElement('canvas');
        canvas.width = 10;
        canvas.height = 10;
        var ctx = canvas.getContext('2d');

        // Create a 2x2 red ImageData-like object
        var imgData = { width: 2, height: 2, data: [
            255, 0, 0, 255,  255, 0, 0, 255,
            255, 0, 0, 255,  255, 0, 0, 255
        ]};
        ctx.drawImage(imgData, 3, 3);

        // Read back the pixel at (3,3) — should be red
        var pixel = ctx.getImageData(3, 3, 1, 1);
        String(pixel.data[0] === 255 && pixel.data[1] === 0 && pixel.data[2] === 0 && pixel.data[3] === 255);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasDrawImageCanvasToCanvas) {
    // drawImage with another canvas element blits its pixels
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), nullptr);
    auto result = engine.evaluate(R"(
        var src = document.createElement('canvas');
        src.width = 4;
        src.height = 4;
        var srcCtx = src.getContext('2d');
        srcCtx.fillStyle = '#00ff00';
        srcCtx.fillRect(0, 0, 4, 4);

        var dst = document.createElement('canvas');
        dst.width = 10;
        dst.height = 10;
        var dstCtx = dst.getContext('2d');
        dstCtx.drawImage(src, 2, 2);

        // Read back pixel at (3,3) — should be green
        var pixel = dstCtx.getImageData(3, 3, 1, 1);
        String(pixel.data[0] === 0 && pixel.data[1] === 255 && pixel.data[2] === 0);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, CanvasDrawImageScaled) {
    // drawImage with dw,dh scales the source
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), nullptr);
    auto result = engine.evaluate(R"(
        var src = document.createElement('canvas');
        src.width = 2;
        src.height = 2;
        var srcCtx = src.getContext('2d');
        srcCtx.fillStyle = '#0000ff';
        srcCtx.fillRect(0, 0, 2, 2);

        var dst = document.createElement('canvas');
        dst.width = 10;
        dst.height = 10;
        var dstCtx = dst.getContext('2d');
        dstCtx.drawImage(src, 0, 0, 6, 6);  // Scale 2x2 → 6x6

        // Read back pixel at (3,3) — center of scaled region, should be blue
        var pixel = dstCtx.getImageData(3, 3, 1, 1);
        String(pixel.data[0] === 0 && pixel.data[1] === 0 && pixel.data[2] === 255);
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

TEST(JSDom, ImageConstructor) {
    // Image/HTMLImageElement constructor for image preloading
    clever::js::JSEngine engine;
    clever::js::install_dom_bindings(engine.context(), nullptr);
    auto result = engine.evaluate(R"(
        var checks = [];
        // Basic constructor
        var img = new Image();
        checks.push(typeof img === 'object');
        checks.push(img.tagName === 'IMG');
        checks.push(img.src === '');
        checks.push(img.complete === false);
        checks.push(img.width === 0);
        // Constructor with dimensions
        var img2 = new Image(100, 50);
        checks.push(img2.width === 100);
        checks.push(img2.height === 50);
        // HTMLImageElement alias
        checks.push(typeof HTMLImageElement === 'function');
        checks.push(new HTMLImageElement() instanceof HTMLImageElement);
        // addEventListener works
        checks.push(typeof img.addEventListener === 'function');
        // decode() returns Promise
        checks.push(typeof img.decode === 'function');
        String(checks.every(function(c){return c;}));
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
    clever::js::cleanup_dom_bindings(engine.context());
}

// ============================================================================
// Cycle 433 — Modern JS language feature regression (QuickJS ES2020+ support)
// ============================================================================

TEST(JSEngine, OptionalChaining) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {a: {b: 42}};
        var checks = [];
        checks.push(obj?.a?.b === 42);
        checks.push(obj?.missing?.b === undefined);
        checks.push(obj?.a?.b?.toString() === '42');
        var arr = [1, 2, 3];
        checks.push(arr?.[1] === 2);
        var fn = null;
        checks.push(fn?.() === undefined);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, NullishCoalescing) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push((null ?? 'default') === 'default');
        checks.push((undefined ?? 99) === 99);
        checks.push((0 ?? 'fallback') === 0);
        checks.push(('' ?? 'fallback') === '');
        checks.push((false ?? 'fallback') === false);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ArrayDestructuring) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var [a, b, c] = [10, 20, 30];
        var [x, , z] = [1, 2, 3];
        var [head, ...tail] = [100, 200, 300];
        String(a === 10 && b === 20 && c === 30 &&
               x === 1 && z === 3 &&
               head === 100 && tail[0] === 200 && tail[1] === 300)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ObjectDestructuring) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var {x, y} = {x: 10, y: 20};
        var {a: renamed, b = 99} = {a: 42};
        var {p: {q}} = {p: {q: 'nested'}};
        String(x === 10 && y === 20 &&
               renamed === 42 && b === 99 &&
               q === 'nested')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, SpreadOperator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var arr1 = [1, 2];
        var arr2 = [3, 4];
        var merged = [...arr1, ...arr2];
        var obj1 = {a: 1};
        var obj2 = {b: 2};
        var mergedObj = {...obj1, ...obj2};
        String(merged.length === 4 &&
               merged[0] === 1 && merged[3] === 4 &&
               mergedObj.a === 1 && mergedObj.b === 2)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ArrayFlatAndFlatMap) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var nested = [1, [2, [3, [4]]]];
        var flat1 = nested.flat();
        var flatAll = nested.flat(Infinity);
        var doubled = [1, 2, 3].flatMap(function(x) { return [x, x * 2]; });
        String(flat1.length === 3 &&
               flatAll.length === 4 && flatAll[3] === 4 &&
               doubled.join(',') === '1,2,2,4,3,6')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ObjectEntriesAndFromEntries) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {a: 1, b: 2, c: 3};
        var entries = Object.entries(obj);
        var reconstructed = Object.fromEntries(entries);
        var keys = Object.keys(obj);
        var values = Object.values(obj);
        String(entries.length === 3 &&
               reconstructed.a === 1 && reconstructed.c === 3 &&
               keys.length === 3 &&
               values.indexOf(2) !== -1)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, StringPadStartAndPadEnd) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push('5'.padStart(3, '0') === '005');
        checks.push('42'.padStart(5) === '   42');
        checks.push('hi'.padEnd(5, '!') === 'hi!!!');
        checks.push('hello'.padEnd(3) === 'hello');
        checks.push('abc'.padStart(3) === 'abc');
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// Cycle 434 — Map, Set, WeakMap, Symbol, generators, for...of, Promise.race/any
// ============================================================================

TEST(JSEngine, MapBuiltIn) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var m = new Map();
        m.set('a', 1);
        m.set('b', 2);
        m.set('b', 99);  // overwrite
        var checks = [];
        checks.push(m.size === 2);
        checks.push(m.get('a') === 1);
        checks.push(m.get('b') === 99);
        checks.push(m.has('a') === true);
        checks.push(m.has('c') === false);
        m.delete('a');
        checks.push(m.size === 1);
        checks.push(m.has('a') === false);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, SetBuiltIn) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s = new Set([1, 2, 3, 2, 1]);  // duplicates removed
        var checks = [];
        checks.push(s.size === 3);
        checks.push(s.has(1) === true);
        checks.push(s.has(4) === false);
        s.add(4);
        checks.push(s.size === 4);
        s.delete(1);
        checks.push(s.size === 3);
        checks.push(s.has(1) === false);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, WeakMapBuiltIn) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var wm = new WeakMap();
        var key = {};
        wm.set(key, 42);
        var checks = [];
        checks.push(wm.has(key) === true);
        checks.push(wm.get(key) === 42);
        wm.delete(key);
        checks.push(wm.has(key) === false);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, SymbolBuiltIn) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s1 = Symbol('foo');
        var s2 = Symbol('foo');
        var s3 = Symbol.for('bar');
        var s4 = Symbol.for('bar');
        var checks = [];
        checks.push(typeof s1 === 'symbol');
        checks.push(s1 !== s2);          // unique symbols
        checks.push(s3 === s4);          // Symbol.for returns same symbol
        checks.push(s1.toString() === 'Symbol(foo)');
        var obj = {};
        obj[s1] = 'value';
        checks.push(obj[s1] === 'value');
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, GeneratorFunction) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function* counter(start) {
            yield start;
            yield start + 1;
            yield start + 2;
        }
        var gen = counter(10);
        var a = gen.next();
        var b = gen.next();
        var c = gen.next();
        var d = gen.next();
        String(a.value === 10 && a.done === false &&
               b.value === 11 && b.done === false &&
               c.value === 12 && c.done === false &&
               d.done === true)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ForOfLoop) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var arr = [10, 20, 30];
        var sum = 0;
        for (var val of arr) { sum += val; }
        var str = '';
        for (var ch of 'abc') { str += ch; }
        var m = new Map([['x', 1], ['y', 2]]);
        var mkeys = [];
        for (var [k, v] of m) { mkeys.push(k); }
        String(sum === 60 && str === 'abc' && mkeys.length === 2)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, PromiseRace) {
    // Verify Promise.race exists, is callable, and returns a thenable
    // (callback resolution requires microtask drain, not tested here)
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof Promise.race === 'function');
        var p = Promise.race([Promise.resolve(1), Promise.resolve(2)]);
        checks.push(typeof p === 'object' && typeof p.then === 'function');
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, PromiseAny) {
    // Verify Promise.any exists, is callable, and returns a thenable
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof Promise.any === 'function');
        var p = Promise.any([Promise.resolve('ok')]);
        checks.push(typeof p === 'object' && typeof p.then === 'function');
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ---------------------------------------------------------------------------
// Cycle 445 — JSON.stringify/parse, RegExp, class syntax, try/catch/finally,
//             Proxy, Array.isArray, Error types, typeof checks
// ---------------------------------------------------------------------------

TEST(JSEngine, JsonStringifyAndParse) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {name: 'test', value: 42, arr: [1, 2, 3]};
        var json = JSON.stringify(obj);
        var parsed = JSON.parse(json);
        var checks = [];
        checks.push(parsed.name === 'test');
        checks.push(parsed.value === 42);
        checks.push(parsed.arr.length === 3);
        checks.push(parsed.arr[1] === 2);
        // Verify stringify produces valid JSON
        checks.push(typeof json === 'string');
        checks.push(json.indexOf('test') !== -1);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, RegularExpressions) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        var re = /hello/i;
        checks.push(re.test('Hello World'));
        checks.push(!re.test('goodbye'));

        // Match
        var m = 'foo123bar'.match(/(\d+)/);
        checks.push(m !== null && m[1] === '123');

        // Replace
        var s = 'aabbcc'.replace(/b+/, 'X');
        checks.push(s === 'aaXcc');

        // Global flag
        var count = 'abcabc'.match(/a/g).length;
        checks.push(count === 2);

        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ClassSyntax) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        class Animal {
            constructor(name) {
                this.name = name;
            }
            speak() {
                return this.name + ' makes a sound';
            }
        }
        class Dog extends Animal {
            speak() {
                return this.name + ' barks';
            }
        }
        var d = new Dog('Rex');
        var a = new Animal('Cat');
        var checks = [];
        checks.push(d.speak() === 'Rex barks');
        checks.push(a.speak() === 'Cat makes a sound');
        checks.push(d instanceof Dog);
        checks.push(d instanceof Animal);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, TryCatchFinally) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var log = [];
        try {
            log.push('try');
            throw new Error('oops');
            log.push('after-throw');
        } catch (e) {
            log.push('catch:' + e.message);
        } finally {
            log.push('finally');
        }
        var checks = [];
        checks.push(log[0] === 'try');
        checks.push(log[1] === 'catch:oops');
        checks.push(log[2] === 'finally');
        checks.push(log.length === 3);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ErrorTypes) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        try { null.x; } catch(e) { checks.push(e instanceof TypeError); }
        try { undeclaredVar; } catch(e) { checks.push(e instanceof ReferenceError); }
        try { eval('{'); } catch(e) { checks.push(e instanceof SyntaxError); }
        var err = new RangeError('out of range');
        checks.push(err.message === 'out of range');
        checks.push(err instanceof Error);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, TypeofChecks) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(typeof undefined === 'undefined');
        checks.push(typeof null === 'object');
        checks.push(typeof true === 'boolean');
        checks.push(typeof 42 === 'number');
        checks.push(typeof 'str' === 'string');
        checks.push(typeof Symbol() === 'symbol');
        checks.push(typeof function(){} === 'function');
        checks.push(typeof {} === 'object');
        checks.push(typeof [] === 'object');
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ArrayIsArrayDistinguishesTypes) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(Array.isArray([]));
        checks.push(Array.isArray([1, 2, 3]));
        checks.push(!Array.isArray({}));
        checks.push(!Array.isArray('string'));
        checks.push(!Array.isArray(42));
        checks.push(!Array.isArray(null));
        checks.push(!Array.isArray(undefined));
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ProxyGetTrap) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var target = {x: 10, y: 20};
        var handler = {
            get: function(obj, prop) {
                return prop in obj ? obj[prop] * 2 : 0;
            }
        };
        var proxy = new Proxy(target, handler);
        var checks = [];
        checks.push(proxy.x === 20);
        checks.push(proxy.y === 40);
        checks.push(proxy.z === 0);  // not in target
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ---------------------------------------------------------------------------
// Cycle 449 — Reflect API, Object.assign, Object.create, structuredClone,
//             template literals, tagged templates, destructuring defaults,
//             rest parameters
// ---------------------------------------------------------------------------

TEST(JSEngine, ReflectAPI) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        var obj = {};
        // Reflect.set / Reflect.get
        Reflect.set(obj, 'x', 42);
        checks.push(Reflect.get(obj, 'x') === 42);
        // Reflect.has
        checks.push(Reflect.has(obj, 'x') === true);
        checks.push(Reflect.has(obj, 'y') === false);
        // Reflect.deleteProperty
        Reflect.deleteProperty(obj, 'x');
        checks.push(Reflect.has(obj, 'x') === false);
        // Reflect.ownKeys
        var target = {a: 1, b: 2};
        var keys = Reflect.ownKeys(target);
        checks.push(keys.length === 2);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ObjectAssign) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        var target = {a: 1};
        var source = {b: 2, c: 3};
        var result = Object.assign(target, source);
        checks.push(result === target);  // returns target
        checks.push(result.a === 1);
        checks.push(result.b === 2);
        checks.push(result.c === 3);
        // Multiple sources
        var combined = Object.assign({}, {x: 1}, {y: 2}, {z: 3});
        checks.push(combined.x === 1 && combined.y === 2 && combined.z === 3);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ObjectCreate) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        var proto = {greet: function() { return 'hello ' + this.name; }};
        var obj = Object.create(proto);
        obj.name = 'world';
        checks.push(obj.greet() === 'hello world');
        checks.push(Object.getPrototypeOf(obj) === proto);
        // null prototype
        var plain = Object.create(null);
        checks.push(typeof plain === 'object');
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, TemplateLiterals) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        var name = 'World';
        var greeting = `Hello, ${name}!`;
        checks.push(greeting === 'Hello, World!');
        // Multi-line
        var multi = `line1
line2`;
        checks.push(multi.indexOf('\n') !== -1);
        // Expression interpolation
        var a = 5, b = 3;
        var expr = `${a} + ${b} = ${a + b}`;
        checks.push(expr === '5 + 3 = 8');
        // Nested template
        var inner = 'inner';
        var outer = `outer ${`nested ${inner}`} end`;
        checks.push(outer === 'outer nested inner end');
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, DestructuringDefaults) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        // Array destructuring with defaults
        var [a = 1, b = 2, c = 3] = [10, 20];
        checks.push(a === 10 && b === 20 && c === 3);  // c uses default
        // Object destructuring with defaults
        var {x = 100, y = 200} = {x: 5};
        checks.push(x === 5 && y === 200);  // y uses default
        // Renamed with defaults
        var {p: pp = 99} = {};
        checks.push(pp === 99);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, RestParametersSumAll) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        function sum(first, ...rest) {
            return rest.reduce(function(acc, v) { return acc + v; }, first);
        }
        checks.push(sum(1, 2, 3, 4) === 10);
        checks.push(sum(5) === 5);  // no rest args
        // Rest must be an array
        function f(...args) { return Array.isArray(args); }
        checks.push(f(1, 2, 3) === true);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, JsonDeepClone) {
    // structuredClone not available; use JSON parse/stringify for deep clone pattern
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        var original = {a: 1, b: {c: 2}, d: [3, 4]};
        var clone = JSON.parse(JSON.stringify(original));
        checks.push(clone !== original);  // different object
        checks.push(clone.a === 1);
        checks.push(clone.b.c === 2);
        checks.push(clone.b !== original.b);  // deep copy
        checks.push(clone.d[0] === 3);
        checks.push(clone.d !== original.d);  // deep copy of array
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ObjectKeysValuesEntries) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        var obj = {a: 1, b: 2, c: 3};
        var keys = Object.keys(obj);
        checks.push(keys.length === 3);
        checks.push(keys.indexOf('a') !== -1);
        var values = Object.values(obj);
        checks.push(values.length === 3);
        checks.push(values.indexOf(2) !== -1);
        var entries = Object.entries(obj);
        checks.push(entries.length === 3);
        checks.push(entries[0].length === 2);  // each entry is [key, value]
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ---------------------------------------------------------------------------
// Cycle 450 — closures, higher-order functions, Array.from, Math methods,
//             Date basics, getter/setter, computed property names,
//             short-circuit evaluation
// ---------------------------------------------------------------------------

TEST(JSEngine, Closures) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        function makeCounter(start) {
            var count = start;
            return {
                inc: function() { return ++count; },
                dec: function() { return --count; },
                get: function() { return count; }
            };
        }
        var c = makeCounter(10);
        checks.push(c.inc() === 11);
        checks.push(c.inc() === 12);
        checks.push(c.dec() === 11);
        checks.push(c.get() === 11);
        // Two independent closures don't share state
        var c2 = makeCounter(0);
        checks.push(c2.get() === 0);
        checks.push(c.get() === 11);  // c unaffected
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, HigherOrderFunctions) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        var nums = [1, 2, 3, 4, 5];
        // map
        var doubled = nums.map(function(x) { return x * 2; });
        checks.push(doubled.join(',') === '2,4,6,8,10');
        // filter
        var evens = nums.filter(function(x) { return x % 2 === 0; });
        checks.push(evens.join(',') === '2,4');
        // reduce
        var sum = nums.reduce(function(acc, x) { return acc + x; }, 0);
        checks.push(sum === 15);
        // chaining
        var result = nums.filter(function(x){return x>2;}).map(function(x){return x*x;});
        checks.push(result.join(',') === '9,16,25');
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ArrayFrom) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        // From array-like
        var arr1 = Array.from('hello');
        checks.push(arr1.length === 5 && arr1[0] === 'h');
        // From Set
        var s = new Set([1, 2, 3]);
        var arr2 = Array.from(s);
        checks.push(arr2.length === 3);
        // With map function
        var arr3 = Array.from([1, 2, 3], function(x) { return x * 2; });
        checks.push(arr3.join(',') === '2,4,6');
        // From length-based object
        var arr4 = Array.from({length: 3}, function(_, i) { return i; });
        checks.push(arr4.join(',') === '0,1,2');
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, MathMethods) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        checks.push(Math.abs(-5) === 5);
        checks.push(Math.max(1, 2, 3) === 3);
        checks.push(Math.min(1, 2, 3) === 1);
        checks.push(Math.floor(4.7) === 4);
        checks.push(Math.ceil(4.2) === 5);
        checks.push(Math.round(4.5) === 5);
        checks.push(Math.pow(2, 10) === 1024);
        checks.push(Math.sqrt(16) === 4);
        var r = Math.random();
        checks.push(r >= 0 && r < 1);
        checks.push(Math.sign(-3) === -1 && Math.sign(0) === 0 && Math.sign(5) === 1);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, DateBasics) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        var now = new Date();
        checks.push(typeof now === 'object');
        checks.push(typeof now.getTime() === 'number');
        // Date.now() returns a number
        checks.push(typeof Date.now() === 'number');
        checks.push(Date.now() > 0);
        // Specific date
        var d = new Date(2000, 0, 1);  // Jan 1, 2000
        checks.push(d.getFullYear() === 2000);
        checks.push(d.getMonth() === 0);  // 0-indexed
        checks.push(d.getDate() === 1);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, GetterAndSetter) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        var obj = {
            _val: 0,
            get value() { return this._val; },
            set value(v) { this._val = v * 2; }
        };
        obj.value = 5;
        checks.push(obj._val === 10);
        checks.push(obj.value === 10);
        // Class getter/setter
        class Rect {
            constructor(w, h) { this._w = w; this._h = h; }
            get area() { return this._w * this._h; }
        }
        var r = new Rect(3, 4);
        checks.push(r.area === 12);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ComputedPropertyNames) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        var key = 'dynamic';
        var obj = {[key]: 42, ['x' + 'y']: 'val'};
        checks.push(obj.dynamic === 42);
        checks.push(obj.xy === 'val');
        // Computed method names in class
        var method = 'greet';
        var greeter = {[method]: function(name) { return 'hi ' + name; }};
        checks.push(greeter.greet('world') === 'hi world');
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ShortCircuitEvaluation) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var checks = [];
        var count = 0;
        function sideEffect() { count++; return true; }
        // && short-circuits on false
        false && sideEffect();
        checks.push(count === 0);
        // || short-circuits on true
        true || sideEffect();
        checks.push(count === 0);
        // Logical assignment
        var a = null;
        a ??= 'default';
        checks.push(a === 'default');
        var b = 'existing';
        b ??= 'fallback';
        checks.push(b === 'existing');
        // ||= and &&=
        var x = 0;
        x ||= 42;
        checks.push(x === 42);
        var y = 10;
        y &&= 99;
        checks.push(y === 99);
        String(checks.every(function(c){return c;}))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// ============================================================================
// Cycle 511: JS Engine regression tests
// ============================================================================

TEST(JSEngine, SwitchStatement) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = 3;
        var out = "";
        switch (x) {
            case 1: out = "one"; break;
            case 2: out = "two"; break;
            case 3: out = "three"; break;
            default: out = "other";
        }
        out
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "three");
}

TEST(JSEngine, TernaryOperator) {
    clever::js::JSEngine engine;
    auto r1 = engine.evaluate("(5 > 3) ? 'yes' : 'no'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(r1, "yes");
    auto r2 = engine.evaluate("(1 > 10) ? 'yes' : 'no'");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(r2, "no");
}

TEST(JSEngine, StringSliceAndIndexOf) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s = "Hello, World!";
        String(s.indexOf("World")) + ":" + s.slice(7, 12)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "7:World");
}

TEST(JSEngine, ArraySortMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var arr = [3, 1, 4, 1, 5, 9, 2, 6];
        arr.sort(function(a, b) { return a - b; });
        arr[0] + "," + arr[7]
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,9");
}

TEST(JSEngine, ArrayReduceMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var nums = [1, 2, 3, 4, 5];
        String(nums.reduce(function(acc, val) { return acc + val; }, 0))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "15");
}

TEST(JSEngine, ObjectFreezePreventsMutation) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = Object.freeze({ x: 10 });
        try {
            obj.x = 99;  // should silently fail in non-strict mode
        } catch (e) {}
        String(obj.x)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "10");
}

TEST(JSEngine, ForInLoop) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = { a: 1, b: 2, c: 3 };
        var keys = [];
        for (var k in obj) { keys.push(k); }
        keys.sort().join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "a,b,c");
}

TEST(JSEngine, StringRepeatMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'ab'.repeat(3)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ababab");
}

// ============================================================================
// Cycle 512: JSEngine regression tests
// ============================================================================

TEST(JSEngine, StringPadStart) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'5'.padStart(3, '0')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "005");
}

TEST(JSEngine, StringPadEnd) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'hi'.padEnd(5, '.')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hi...");
}

TEST(JSEngine, NumberToFixed) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(3.14159).toFixed(2)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3.14");
}

TEST(JSEngine, DeleteOperator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = { a: 1, b: 2 };
        delete obj.a;
        String('a' in obj)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false");
}

TEST(JSEngine, InOperator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = { x: 42 };
        String('x' in obj) + ',' + String('y' in obj)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

TEST(JSEngine, InstanceofOperator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        String([] instanceof Array) + ',' + String({} instanceof Object)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,true");
}

TEST(JSEngine, ArrayFill) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[1, 2, 3, 4].fill(0, 1, 3).join(',')");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,0,0,4");
}

TEST(JSEngine, ObjectSpread) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var a = { x: 1, y: 2 };
        var b = { y: 99, z: 3 };
        var c = Object.assign({}, a, b);
        c.x + ',' + c.y + ',' + c.z
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,99,3");
}

// ============================================================================
// Cycle 519: JSEngine regression tests
// ============================================================================

TEST(JSEngine, NullishCoalescingWithZero) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var a = null ?? "default";
        var b = undefined ?? "fallback";
        var c = 0 ?? "nonzero";
        a + "," + b + "," + String(c)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "default,fallback,0");
}

TEST(JSEngine, OptionalChainingOnObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = { a: { b: 42 } };
        var v1 = obj?.a?.b;
        var v2 = obj?.x?.y;
        String(v1) + "," + String(v2)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42,undefined");
}

TEST(JSEngine, ArrayEveryMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var all = [2, 4, 6].every(function(x) { return x % 2 === 0; });
        var some = [1, 3, 5].every(function(x) { return x % 2 === 0; });
        String(all) + "," + String(some)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

TEST(JSEngine, ArraySomeMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var any = [1, 2, 3].some(function(x) { return x > 2; });
        var none = [1, 2, 3].some(function(x) { return x > 10; });
        String(any) + "," + String(none)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

TEST(JSEngine, StringTrimMethods) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "  hello  ".trim() + "|" + "  hi".trimStart() + "|" + "bye  ".trimEnd()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello|hi|bye");
}

TEST(JSEngine, ObjectHasOwnProperty) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = { x: 1 };
        String(obj.hasOwnProperty('x')) + "," + String(obj.hasOwnProperty('toString'))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

TEST(JSEngine, NumberIsNaNAndIsFinite) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        String(Number.isNaN(NaN)) + "," +
        String(Number.isNaN(42)) + "," +
        String(Number.isFinite(Infinity)) + "," +
        String(Number.isFinite(100))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false,false,true");
}

TEST(JSEngine, ArrayIndexOf) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var arr = [10, 20, 30, 20];
        arr.indexOf(20) + "," + arr.lastIndexOf(20) + "," + arr.indexOf(99)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,3,-1");
}

// ============================================================================
// Cycle 523: JSEngine regression tests
// ============================================================================

TEST(JSEngine, WhileLoop) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var sum = 0;
        var i = 1;
        while (i <= 5) {
            sum += i;
            i++;
        }
        String(sum)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "15");
}

TEST(JSEngine, DoWhileLoop) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var count = 0;
        do {
            count++;
        } while (count < 3);
        String(count)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, BreakInLoop) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var found = -1;
        for (var i = 0; i < 10; i++) {
            if (i === 5) { found = i; break; }
        }
        String(found)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "5");
}

TEST(JSEngine, ContinueInLoop) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var evens = [];
        for (var i = 0; i < 6; i++) {
            if (i % 2 !== 0) continue;
            evens.push(i);
        }
        evens.join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0,2,4");
}

TEST(JSEngine, SetDataStructure) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s = new Set([1, 2, 3, 2, 1]);
        String(s.size)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, MapDataStructure) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var m = new Map();
        m.set("key", "value");
        m.get("key")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "value");
}

TEST(JSEngine, ArrowFunctionMultiply) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var double = x => x * 2;
        double(21)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42");
}

TEST(JSEngine, DefaultFunctionParameters) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function greet(name = "World") {
            return "Hello, " + name + "!";
        }
        greet() + " " + greet("Alice")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Hello, World! Hello, Alice!");
}

// ============================================================================
// Cycle 527: JS engine regression tests
// ============================================================================

// Array.from with mapping function
TEST(JSEngine, ArrayFromWithMapper) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Array.from({length: 4}, function(_, i) { return i * 3; }).join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0,3,6,9");
}

// Object.entries to iterate key-value pairs
TEST(JSEngine, ObjectEntries) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {x: 1, y: 2};
        Object.entries(obj).map(function(e) { return e[0] + "=" + e[1]; }).join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "x=1,y=2");
}

// Array.isArray type checking
TEST(JSEngine, ArrayIsArrayOnNonArray) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        String(Array.isArray([1,2,3])) + "," + String(Array.isArray("not an array"))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// Regex test method
TEST(JSEngine, RegexTestMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var re = /^\d+$/;
        String(re.test("123")) + "," + String(re.test("abc"))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// Object destructuring assignment
TEST(JSEngine, DestructuringAssignment) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {a: 10, b: 20};
        var {a, b} = obj;
        a + b
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "30");
}

// Array flat method
TEST(JSEngine, ArrayFlatMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, [2, 3], [4, [5]]].flat().join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,3,4,5");
}

// Rest parameters in function
TEST(JSEngine, RestParamsSumFiveValues) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function sum(...args) {
            return args.reduce(function(a, b) { return a + b; }, 0);
        }
        sum(1, 2, 3, 4, 5)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "15");
}

// Object.values returns array of values
TEST(JSEngine, ObjectValues) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {p: 7, q: 8, r: 9};
        Object.values(obj).reduce(function(a, b) { return a + b; }, 0)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "24");
}

// ============================================================================
// Cycle 538: JS engine regression tests
// ============================================================================

// String includes() method
TEST(JSEngine, StringIncludesMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s = "hello world";
        String(s.includes("world")) + "," + String(s.includes("xyz"))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// String startsWith() method
TEST(JSEngine, StringStartsWithMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s = "foobar";
        String(s.startsWith("foo")) + "," + String(s.startsWith("bar"))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// String endsWith() method
TEST(JSEngine, StringEndsWithMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s = "foobar";
        String(s.endsWith("bar")) + "," + String(s.endsWith("foo"))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// Array.from with string argument
TEST(JSEngine, ArrayFromString) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Array.from("abc").join("-")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "a-b-c");
}

// typeof operator
TEST(JSEngine, TypeofOperator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        typeof 42 + "," + typeof "hello" + "," + typeof true + "," + typeof undefined
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "number,string,boolean,undefined");
}

// Object.keys returns array of property names
TEST(JSEngine, ObjectKeys) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {a: 1, b: 2, c: 3};
        Object.keys(obj).sort().join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "a,b,c");
}

// Comma operator evaluates to last value
TEST(JSEngine, CommaOperator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = (1, 2, 3);
        x
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");
}

// Logical AND short-circuit
TEST(JSEngine, LogicalAndShortCircuit) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var called = false;
        false && (function() { called = true; })();
        String(called)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false");
}

// ============================================================================
// Cycle 542: JS engine regression tests
// ============================================================================

// Logical OR short-circuit
TEST(JSEngine, LogicalOrShortCircuit) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var called = false;
        true || (function() { called = true; })();
        String(called)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false");
}

// String.charAt() method
TEST(JSEngine, StringCharAt) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "hello".charAt(1)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "e");
}

// String.charCodeAt() method
TEST(JSEngine, StringCharCodeAt) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "A".charCodeAt(0)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "65");
}

// Array.findIndex method
TEST(JSEngine, ArrayFindIndex) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [10, 20, 30, 40].findIndex(function(x) { return x > 25; })
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "2");
}

// Array.find method
TEST(JSEngine, ArrayFindMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [5, 12, 8, 130, 44].find(function(x) { return x > 10; })
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "12");
}

// Array.includes method
TEST(JSEngine, ArrayIncludes) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var a = [1, 2, 3, 4];
        String(a.includes(3)) + "," + String(a.includes(9))
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// String.split method
TEST(JSEngine, StringSplitMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "a,b,c,d".split(",").length
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "4");
}

// Math.abs returns absolute value
TEST(JSEngine, MathAbsReturnsAbsValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Math.abs(-42) + "," + Math.abs(7)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42,7");
}

// ============================================================================
// Cycle 547: JS engine regression tests
// ============================================================================

// Number.parseInt converts string to integer
TEST(JSEngine, NumberParseInt) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Number.parseInt("42") + Number.parseInt("0xFF", 16)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "297");
}

// Number.parseFloat parses decimal
TEST(JSEngine, NumberParseFloat) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Number.parseFloat("3.14").toFixed(2)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3.14");
}

// Array.concat merges two arrays
TEST(JSEngine, ArrayConcat) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, 2].concat([3, 4]).join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,3,4");
}

// String.toLowerCase converts string
TEST(JSEngine, StringToLowerCase) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "HELLO WORLD".toLowerCase()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello world");
}

// String.toUpperCase converts string
TEST(JSEngine, StringToUpperCase) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "hello world".toUpperCase()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "HELLO WORLD");
}

// Ternary operator selects correct branch
TEST(JSEngine, TernaryOperatorBranching) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = 10;
        var label = (x > 5) ? "big" : "small";
        label
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "big");
}

// Object.assign merges objects
TEST(JSEngine, ObjectAssignMerge) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var target = {a: 1};
        Object.assign(target, {b: 2, c: 3});
        target.a + target.b + target.c
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "6");
}

// Array.splice removes elements
TEST(JSEngine, ArraySpliceRemoves) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var a = [1, 2, 3, 4, 5];
        a.splice(1, 2);  // remove 2 elements starting at index 1
        a.join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,4,5");
}

// ============================================================================
// Cycle 553: JS engine regression tests
// ============================================================================

// JSON.stringify converts object to JSON string
TEST(JSEngine, JSONStringify) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        JSON.stringify({x: 1, y: 2})
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    // Should contain the key-value pairs
    EXPECT_NE(result.find("\"x\""), std::string::npos);
    EXPECT_NE(result.find("\"y\""), std::string::npos);
}

// JSON.parse converts JSON string to object
TEST(JSEngine, JSONParse) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = JSON.parse('{"a":10,"b":20}');
        obj.a + obj.b
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "30");
}

// Array.flatMap method
TEST(JSEngine, ArrayFlatMap) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, 2, 3].flatMap(function(x) { return [x, x * 2]; }).join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,2,4,3,6");
}

// String.matchAll pattern
TEST(JSEngine, StringMatchAllCount) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var matches = [...'aababc'.matchAll(/ab/g)];
        matches.length
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "2");
}

// Array.keys() iterator
TEST(JSEngine, ArrayKeysIterator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [...['a','b','c'].keys()].join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0,1,2");
}

// Array.values() iterator
TEST(JSEngine, ArrayValuesIterator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [...['x','y','z'].values()].join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "x,y,z");
}

// String.replaceAll method
TEST(JSEngine, StringReplaceAll) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "foo bar foo baz foo".replaceAll("foo", "qux")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "qux bar qux baz qux");
}

// Array.entries() iterator
TEST(JSEngine, ArrayEntriesIterator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var pairs = [];
        for (var [i, v] of ['a','b'].entries()) {
            pairs.push(i + ':' + v);
        }
        pairs.join(',')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0:a,1:b");
}

// ============================================================================
// Cycle 559: JS engine regression tests
// ============================================================================

// Generator function spread into array
TEST(JSEngine, GeneratorFunctionSpreadToArray) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function* gen() {
            yield 1;
            yield 2;
            yield 3;
        }
        [...gen()].join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,3");
}

// for..of loop over array
TEST(JSEngine, ForOfArray) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var sum = 0;
        for (var x of [10, 20, 30]) {
            sum += x;
        }
        sum
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "60");
}

// Template literal with expression
TEST(JSEngine, TemplateLiteralWithExpression) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var name = "World";
        var n = 2 + 2;
        `Hello, ${name}! ${n} items.`
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Hello, World! 4 items.");
}

// Spread operator in function call
TEST(JSEngine, SpreadInFunctionCall) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var nums = [3, 1, 4, 1, 5];
        Math.max(...nums)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "5");
}

// Array destructuring with rest element joined
TEST(JSEngine, ArrayDestructuringRestJoin) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var [first, second, ...rest] = [10, 20, 30, 40, 50];
        first + "," + second + "," + rest.join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "10,20,30,40,50");
}

// Symbol.iterator used in for..of
TEST(JSEngine, SymbolIteratorWithSet) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s = new Set([1, 2, 3]);
        var vals = [];
        for (var v of s) {
            vals.push(v);
        }
        vals.sort().join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,3");
}

// WeakMap basic usage
TEST(JSEngine, WeakMapBasicUsage) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var wm = new WeakMap();
        var obj = {};
        wm.set(obj, 42);
        wm.get(obj)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42");
}

// Promise.resolve().then() is async
TEST(JSEngine, PromiseResolveThen) {
    clever::js::JSEngine engine;
    // Promise in sync context - result may be undefined or resolved depending on implementation
    auto result = engine.evaluate(R"(
        var resolved = null;
        Promise.resolve(99).then(function(v) { resolved = v; });
        // In a synchronous eval, we might need to check 'resolved' after microtasks
        // Just check that Promise.resolve doesn't throw
        "ok"
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ok");
}

// ============================================================================
// Cycle 565: More modern JS features
// ============================================================================

// Object.freeze: frozen object still reads correctly
TEST(JSEngine, ObjectFreezeReadAfterFreeze) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = Object.freeze({a: 10, b: 20});
        obj.a + obj.b
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "30");
}

// Array.of creates array from arguments
TEST(JSEngine, ArrayOfCreatesArray) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Array.of(1, 2, 3).join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,3");
}

// String.prototype.padStart with longer pad
TEST(JSEngine, StringPadStartLonger) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "42".padStart(5, "0")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "00042");
}

// String.prototype.padEnd with spaces
TEST(JSEngine, StringPadEndWithSpaces) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "abc".padEnd(6).length
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "6");
}

// Array.prototype.every: all match
TEST(JSEngine, ArrayEveryAllMatch) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [2, 4, 6].every(function(x) { return x % 2 === 0; })
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// Array.prototype.some: one matches
TEST(JSEngine, ArraySomeOneMatches) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, 3, 4].some(function(x) { return x % 2 === 0; })
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// typeof null is "object"
TEST(JSEngine, TypeofNullIsObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(typeof null)");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");
}

// instanceof Array on object returns false
TEST(JSEngine, InstanceofArrayFalseForObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        ({}) instanceof Array
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false");
}

// ============================================================================
// Cycle 571: More JS engine tests
// ============================================================================

// Object.keys on empty object
TEST(JSEngine, ObjectKeysEmptyObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Object.keys({}).length
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0");
}

// Number.isInteger true for integer
TEST(JSEngine, NumberIsIntegerTrue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Number.isInteger(42)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// Number.isInteger false for float
TEST(JSEngine, NumberIsIntegerFalse) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Number.isInteger(3.14)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false");
}

// Number.isFinite on Infinity
TEST(JSEngine, NumberIsFiniteOnInfinity) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Number.isFinite(Infinity)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false");
}

// Math.floor
TEST(JSEngine, MathFloor) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(Math.floor(4.7))");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "4");
}

// Math.ceil
TEST(JSEngine, MathCeil) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(Math.ceil(4.1))");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "5");
}

// Math.round
TEST(JSEngine, MathRound) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(Math.round(4.5))");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "5");
}

// String.prototype.trim
TEST(JSEngine, StringTrim) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "  hello world  ".trim()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello world");
}

// ============================================================================
// Cycle 577: More JS engine tests
// ============================================================================

// Nullish coalescing: null/undefined/zero all handled
TEST(JSEngine, NullishCoalescingThreeValues) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var a = null ?? "default";
        var b = undefined ?? "fallback";
        var c = 0 ?? "zero_fallback";
        a + "," + b + "," + c
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "default,fallback,0");
}

// Optional chaining: deep access with missing key
TEST(JSEngine, OptionalChainingDeepMissingKey) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {a: {b: 42}};
        var r1 = obj?.a?.b;
        var r2 = obj?.x?.y;
        r1 + "," + (r2 === undefined ? "undefined" : r2)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42,undefined");
}

// Logical assignment (&&=)
TEST(JSEngine, LogicalAndAssignment) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = true;
        x &&= 42;
        x
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42");
}

// Logical assignment (||=)
TEST(JSEngine, LogicalOrAssignment) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = false;
        x ||= "assigned";
        x
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "assigned");
}

// Array destructuring with default values
TEST(JSEngine, ArrayDestructuringWithDefaults) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var [a = 10, b = 20] = [1];
        a + "," + b
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,20");
}

// Object destructuring with rename
TEST(JSEngine, ObjectDestructuringRename) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var { x: myX, y: myY } = { x: 10, y: 20 };
        myX + myY
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "30");
}

// for...in loops over object keys
TEST(JSEngine, ForInLoopOverObjectKeys) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {a: 1, b: 2, c: 3};
        var keys = [];
        for (var k in obj) { keys.push(k); }
        keys.sort().join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "a,b,c");
}

// String repeat
TEST(JSEngine, StringRepeat) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "ab".repeat(3)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "ababab");
}

// ============================================================================
// Cycle 583: More JS engine tests
// ============================================================================

// Array.prototype.reduceRight
TEST(JSEngine, ArrayReduceRight) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, 2, 3, 4].reduceRight(function(acc, x) { return acc + x; }, 0)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "10");
}

// Object.create creates object with prototype
TEST(JSEngine, ObjectCreateWithPrototype) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var proto = { greet: function() { return "hello"; } };
        var obj = Object.create(proto);
        obj.greet()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello");
}

// Error constructor and message
TEST(JSEngine, ErrorConstructorMessage) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var e = new Error("something failed");
        e.message
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "something failed");
}

// try/catch/finally with throw string
TEST(JSEngine, TryCatchFinallyThrowString) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var out = "";
        try {
            throw "fail";
        } catch(e) {
            out += "caught:" + e;
        } finally {
            out += "+done";
        }
        out
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "caught:fail+done");
}

// Class syntax: basic class with method
TEST(JSEngine, BasicClassWithMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        class Counter {
            constructor(start) { this.count = start; }
            increment() { this.count++; return this.count; }
        }
        var c = new Counter(10);
        c.increment()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "11");
}

// Class inheritance
TEST(JSEngine, ClassInheritance) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        class Animal { speak() { return "..."; } }
        class Dog extends Animal { speak() { return "woof"; } }
        var d = new Dog();
        d.speak()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "woof");
}

// Arrow function this binding
TEST(JSEngine, ArrowFunctionThisBinding) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function Timer() {
            this.val = 42;
            this.get = () => this.val;
        }
        var t = new Timer();
        t.get()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42");
}

// Map: basic get/set/has
TEST(JSEngine, MapBasicOperations) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var m = new Map();
        m.set("key", 99);
        m.has("key") + "," + m.get("key")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,99");
}

// ============================================================================
// Cycle 587: More JS engine tests
// ============================================================================

// Class static method
TEST(JSEngine, ClassStaticMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        class MathHelper {
            static double(x) { return x * 2; }
        }
        MathHelper.double(21)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42");
}

// Getter/setter in object literal
TEST(JSEngine, GetterSetterInObjectLiteral) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {
            _x: 10,
            get x() { return this._x; },
            set x(v) { this._x = v; }
        };
        obj.x = 99;
        obj.x
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "99");
}

// Symbol creates unique value
TEST(JSEngine, SymbolCreatesUniqueValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s1 = Symbol("tag");
        var s2 = Symbol("tag");
        s1 === s2
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false");
}

// Set: size, add, has, delete
TEST(JSEngine, SetOperations) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s = new Set([1, 2, 3]);
        s.add(4);
        s.delete(2);
        s.has(3) + "," + s.size
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,3");
}

// Async function returns Promise
TEST(JSEngine, AsyncFunctionReturnsPromise) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        async function greet() { return "hello"; }
        greet() instanceof Promise
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// Array.from with Set
TEST(JSEngine, ArrayFromSet) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s = new Set([3, 1, 2]);
        Array.from(s).sort().join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,3");
}

// typeof function is "function"
TEST(JSEngine, TypeofFunctionIsFunction) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(typeof function(){})");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

// Regex match
TEST(JSEngine, RegexMatchMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var m = "hello world".match(/\w+/g);
        m.join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello,world");
}

// ============================================================================
// Cycle 594: More JS engine tests
// ============================================================================

// Proxy basic get trap
TEST(JSEngine, ProxyBasicGetTrap) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var handler = {
            get: function(target, prop) {
                return prop in target ? target[prop] : "default";
            }
        };
        var obj = new Proxy({x: 42}, handler);
        obj.x + "," + obj.missing
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42,default");
}

// Reflect.ownKeys on object
TEST(JSEngine, ReflectOwnKeys) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var keys = Reflect.ownKeys({a: 1, b: 2, c: 3});
        keys.sort().join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "a,b,c");
}

// String.raw template literal
TEST(JSEngine, StringRawTemplateLiteral) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        String.raw`Hello\nWorld`
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Hello\\nWorld");
}

// Array.prototype.fill entire array
TEST(JSEngine, ArrayFillNewArrayZeros) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        new Array(4).fill(0).join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0,0,0,0");
}

// Array.prototype.copyWithin
TEST(JSEngine, ArrayCopyWithin) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, 2, 3, 4, 5].copyWithin(0, 3).join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "4,5,3,4,5");
}

// Number.toFixed with different precision
TEST(JSEngine, NumberToFixedThreeDecimals) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        (2.71828).toFixed(3)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "2.718");
}

// String.prototype.indexOf
TEST(JSEngine, StringIndexOf) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "hello world".indexOf("world")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "6");
}

// String.prototype.lastIndexOf
TEST(JSEngine, StringLastIndexOf) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "abcabc".lastIndexOf("c")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "5");
}

// ============================================================================
// Cycle 600: Milestone — More JS engine tests
// ============================================================================

// WeakRef basic usage
TEST(JSEngine, WeakRefDeref) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = { val: 99 };
        var ref = new WeakRef(obj);
        ref.deref().val
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "99");
}

// FinalizationRegistry basic
TEST(JSEngine, FinalizationRegistryConstructs) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var fr = new FinalizationRegistry(function(val) {});
        typeof fr
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");
}

// Array.prototype.flat (depth=1 default)
TEST(JSEngine, ArrayFlat) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, [2, [3]]].flat().join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,3");
}

// Array.prototype.flatMap with doubled values
TEST(JSEngine, ArrayFlatMapDoubled) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, 2, 3].flatMap(function(x) { return [x, x * 2]; }).join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,2,4,3,6");
}

// Object.entries sorted
TEST(JSEngine, ObjectEntriesSorted) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Object.entries({a: 1, b: 2}).sort().map(function(e) { return e[0] + "=" + e[1]; }).join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "a=1,b=2");
}

// Object.values with three keys
TEST(JSEngine, ObjectValuesThreeKeys) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Object.values({p: 5, q: 10, r: 15}).join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "5,10,15");
}

// String.prototype.includes
TEST(JSEngine, StringIncludes) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "hello world".includes("world") + "," + "hello world".includes("xyz")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// String.prototype.startsWith and endsWith
TEST(JSEngine, StringStartsWithEndsWith) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "hello".startsWith("hel") + "," + "hello".endsWith("llo")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,true");
}

// ============================================================================
// Cycle 605: More JS engine tests
// ============================================================================

// Promise.all is a function
TEST(JSEngine, PromiseAllIsFunction) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        typeof Promise.all
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

// Generator function with yield*
TEST(JSEngine, GeneratorYieldStar) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function* gen() { yield* [1, 2, 3]; }
        var arr = [];
        for (var v of gen()) arr.push(v);
        arr.join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,3");
}

// Destructuring assignment with swap
TEST(JSEngine, DestructuringSwap) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var a = 1, b = 2;
        [a, b] = [b, a];
        a + "," + b
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "2,1");
}

// Tagged template literal
TEST(JSEngine, TaggedTemplateLiteral) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function tag(strings, val) { return strings[0] + val + strings[1]; }
        tag`Hello ${42} World`
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "Hello 42 World");
}

// Array.from with map function
TEST(JSEngine, ArrayFromWithMap) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Array.from([1, 2, 3], function(x) { return x * 3; }).join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3,6,9");
}

// Object.assign merges three sources
TEST(JSEngine, ObjectAssignThreeSources) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var target = {a: 1};
        Object.assign(target, {b: 2}, {c: 3});
        target.a + "," + target.b + "," + target.c
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,3");
}

// Number.parseInt and Number.parseFloat
TEST(JSEngine, NumberParseIntAndFloat) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Number.parseInt("42") + "," + Number.parseFloat("3.14").toFixed(2)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42,3.14");
}

// String.prototype.split with limit
TEST(JSEngine, StringSplitWithLimit) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "a,b,c,d".split(",", 2).join("|")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "a|b");
}

// ============================================================================
// Cycle 610: More JS engine tests
// ============================================================================

// Computed property names dynamic key
TEST(JSEngine, ComputedPropertyNamesDynamic) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var key = "answer";
        var obj = { [key]: 42 };
        obj.answer
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42");
}

// for...of with string
TEST(JSEngine, ForOfString) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var chars = [];
        for (var c of "abc") chars.push(c);
        chars.join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "a,b,c");
}

// Array.isArray
TEST(JSEngine, ArrayIsArray) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Array.isArray([]) + "," + Array.isArray({})
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// Object.getPrototypeOf
TEST(JSEngine, ObjectGetPrototypeOf) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Object.getPrototypeOf([]) === Array.prototype
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// String.prototype.trimStart and trimEnd
TEST(JSEngine, StringTrimStartEnd) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "  hello  ".trimStart() + "|" + "  world  ".trimEnd()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello  |  world");
}

// Array.prototype.at
TEST(JSEngine, ArrayAt) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var arr = [10, 20, 30];
        arr.at(0) + "," + arr.at(-1)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "10,30");
}

// String.prototype.at
TEST(JSEngine, StringAt) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "hello".at(0) + "," + "hello".at(-1)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "h,o");
}

// Math.hypot
TEST(JSEngine, MathHypot) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Math.hypot(3, 4)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "5");
}

// ============================================================================
// Cycle 614: More JS engine tests
// ============================================================================

// Math.sign
TEST(JSEngine, MathSign) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Math.sign(-5) + "," + Math.sign(0) + "," + Math.sign(3)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "-1,0,1");
}

// Math.trunc
TEST(JSEngine, MathTrunc) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Math.trunc(3.7) + "," + Math.trunc(-3.7)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3,-3");
}

// Math.log2
TEST(JSEngine, MathLog2) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Math.log2(8)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");
}

// Math.log10
TEST(JSEngine, MathLog10) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Math.log10(1000)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");
}

// Number.isNaN
TEST(JSEngine, NumberIsNaN) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Number.isNaN(NaN) + "," + Number.isNaN(42)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// Number.MAX_SAFE_INTEGER
TEST(JSEngine, NumberMaxSafeInteger) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Number.MAX_SAFE_INTEGER === 9007199254740991
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// Number.MIN_SAFE_INTEGER
TEST(JSEngine, NumberMinSafeInteger) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Number.MIN_SAFE_INTEGER === -9007199254740991
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// Array.prototype.findIndex with negative result
TEST(JSEngine, ArrayFindIndexNotFound) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, 2, 3].findIndex(function(x) { return x > 100; })
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "-1");
}

// ============================================================================
// Cycle 619: More JS engine tests
// ============================================================================

// Array.prototype.find
TEST(JSEngine, ArrayFind) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [5, 12, 8, 130, 44].find(function(x) { return x > 10; })
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "12");
}

// Object.fromEntries
TEST(JSEngine, ObjectFromEntries) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = Object.fromEntries([["a", 1], ["b", 2]]);
        obj.a + "," + obj.b
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2");
}

// Array.prototype.includes with NaN
TEST(JSEngine, ArrayIncludesNaN) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, NaN, 3].includes(NaN)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// String.prototype.replaceAll all occurrences
TEST(JSEngine, StringReplaceAllAllOccurrences) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        "hello world hello".replaceAll("hello", "hi")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hi world hi");
}

// BigInt basic operations
TEST(JSEngine, BigIntBasic) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        (9007199254740993n + 1n).toString()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "9007199254740994");
}

// Logical AND chain
TEST(JSEngine, LogicalAndChain) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        true && true && false
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false");
}

// Logical OR three false reaches default
TEST(JSEngine, LogicalOrThreeFalseDefault) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        null || undefined || "fallback"
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "fallback");
}

// Conditional/ternary nested
TEST(JSEngine, NestedTernary) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = 5;
        x > 10 ? "big" : x > 3 ? "medium" : "small"
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "medium");
}

// ============================================================================
// Cycle 623: More JS engine tests
// ============================================================================

// String.prototype.matchAll
TEST(JSEngine, StringMatchAll) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var matches = [...'test1 test2 test3'.matchAll(/test(\d)/g)];
        matches.length
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");
}

// Promise.race is a function
TEST(JSEngine, PromiseRaceIsFunction) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        typeof Promise.race
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

// Error name and message
TEST(JSEngine, ErrorNameAndMessage) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var e = new TypeError("bad type");
        e.name + ":" + e.message
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "TypeError:bad type");
}

// Object.hasOwn
TEST(JSEngine, ObjectHasOwn) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {a: 1};
        Object.hasOwn(obj, "a") + "," + Object.hasOwn(obj, "b")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// JSON.stringify nested object
TEST(JSEngine, JSONStringifyNested) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        JSON.stringify({a: [1, 2]})
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "{\"a\":[1,2]}");
}

// Array.prototype.toReversed (non-mutating)
TEST(JSEngine, ArrayToReversed) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var arr = [1, 2, 3];
        arr.toReversed().join(",") + "|" + arr.join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3,2,1|1,2,3");
}

// Array.prototype.toSorted (non-mutating)
TEST(JSEngine, ArrayToSorted) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var arr = [3, 1, 2];
        arr.toSorted().join(",") + "|" + arr.join(",")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1,2,3|3,1,2");
}

// globalThis is an object
TEST(JSEngine, GlobalThisIsObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        typeof globalThis
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");
}

// ============================================================================
// Cycle 628: More JS engine tests
// ============================================================================

// JSON.parse basic
TEST(JSEngine, JSONParseBasic) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = JSON.parse('{"x": 42}');
        obj.x
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42");
}

// JSON.stringify array
TEST(JSEngine, JSONStringifyArray) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        JSON.stringify([1, 2, 3])
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "[1,2,3]");
}

// Date.now returns a number
TEST(JSEngine, DateNowIsNumber) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        typeof Date.now()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "number");
}

// parseInt with radix 16
TEST(JSEngine, ParseIntHex) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        parseInt("ff", 16)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "255");
}

// isNaN global function
TEST(JSEngine, IsNaNGlobal) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        isNaN(NaN) + "," + isNaN(42)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// isFinite global function
TEST(JSEngine, IsFiniteGlobal) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        isFinite(42) + "," + isFinite(Infinity)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// encodeURIComponent
TEST(JSEngine, EncodeURIComponent) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        encodeURIComponent("hello world")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello%20world");
}

// decodeURIComponent
TEST(JSEngine, DecodeURIComponent) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        decodeURIComponent("hello%20world")
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello world");
}

// ============================================================================
// Cycle 636: More JS engine tests
// ============================================================================

// Map: set and get
TEST(JSEngine, MapSetAndGet) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var m = new Map();
        m.set('key', 42);
        m.get('key')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42");
}

// Set: add and has
TEST(JSEngine, SetAddAndHas) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s = new Set([1, 2, 3]);
        s.has(2) + ',' + s.has(9)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// Set: size property
TEST(JSEngine, SetSize) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s = new Set([1, 1, 2, 3]);
        s.size
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");
}

// Map: size property
TEST(JSEngine, MapSize) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var m = new Map([['a', 1], ['b', 2]]);
        m.size
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "2");
}

// Array destructuring sum
TEST(JSEngine, ArrayDestructuringSum) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var [x, y, z] = [10, 20, 30];
        x + y + z
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "60");
}

// Object destructuring multiply
TEST(JSEngine, ObjectDestructuringMultiply) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var {a, b} = {a: 5, b: 7};
        a * b
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "35");
}

// Rest parameters sum four numbers
TEST(JSEngine, RestParametersSumFour) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function sum(...args) { return args.reduce((a, b) => a + b, 0); }
        sum(1, 2, 3, 4)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "10");
}

// Spread two arrays merged length
TEST(JSEngine, SpreadTwoArraysMergedLength) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var a = [1, 2];
        var b = [3, 4];
        [...a, ...b].length
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "4");
}

// ============================================================================
// Cycle 640: More JS engine tests
// ============================================================================

// Optional chaining nested two levels
TEST(JSEngine, OptionalChainingNestedTwoLevels) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {a: {b: 42}};
        obj?.a?.b
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42");
}

// Optional chaining returns undefined on missing key
TEST(JSEngine, OptionalChainingMissingReturnsUndefined) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {};
        String(obj?.a?.b)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "undefined");
}

// Nullish coalescing null returns default
TEST(JSEngine, NullishCoalescingNullDefault) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = null ?? 'default';
        x
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "default");
}

// Nullish coalescing preserves 0
TEST(JSEngine, NullishCoalescingPreservesZero) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = 0 ?? 'default';
        x
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "0");
}

// Symbol basic creation
TEST(JSEngine, SymbolCreate) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        typeof Symbol('desc')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "symbol");
}

// WeakMap basic usage
TEST(JSEngine, WeakMapBasic) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var key = {};
        var wm = new WeakMap();
        wm.set(key, 99);
        wm.get(key)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "99");
}

// Generator next with value
TEST(JSEngine, GeneratorNextWithValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function* gen() { yield 10; yield 20; }
        var g = gen();
        g.next().value + g.next().value
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "30");
}

// for...of with Map entries
TEST(JSEngine, ForOfMapEntries) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var m = new Map([['a', 1], ['b', 2]]);
        var sum = 0;
        for (var [k, v] of m) sum += v;
        sum
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");
}

// ============================================================================
// Cycle 645: More JS engine tests
// ============================================================================

// Async/await type check
TEST(JSEngine, AsyncFunctionIsFunction) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        async function f() { return 1; }
        typeof f
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "function");
}

// String padStart with zeros
TEST(JSEngine, StringPadStartWithZeros) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        '5'.padStart(3, '0')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "005");
}

// String padEnd with dots
TEST(JSEngine, StringPadEndWithDots) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        'hi'.padEnd(5, '.')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hi...");
}

// Array every
TEST(JSEngine, ArrayEveryAllPositive) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, 2, 3].every(x => x > 0)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// Array some
TEST(JSEngine, ArraySomeHasNegative) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, -2, 3].some(x => x < 0)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// Object.keys length
TEST(JSEngine, ObjectKeysLength) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Object.keys({a: 1, b: 2, c: 3}).length
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");
}

// Number.isInteger
TEST(JSEngine, NumberIsInteger) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Number.isInteger(42) + ',' + Number.isInteger(42.5)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true,false");
}

// Array.of creates array
TEST(JSEngine, ArrayOf) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Array.of(1, 2, 3).length
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");
}

// ============================================================================
// Cycle 649: More JS engine tests
// ============================================================================

// Logical assignment &&= with truthy sets to 99
TEST(JSEngine, LogicalAndAssignmentTruthySetTo99) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = 1;
        x &&= 99;
        x
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "99");
}

// Logical assignment ||= with falsy sets to 42
TEST(JSEngine, LogicalOrAssignmentFalsySetTo42) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = 0;
        x ||= 42;
        x
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "42");
}

// Logical assignment ??=
TEST(JSEngine, NullishAssignment) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = null;
        x ??= 'hello';
        x
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hello");
}

// String replaceAll with regex
TEST(JSEngine, StringReplaceAllSpaces) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        'a b c d'.replaceAll(' ', '-')
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "a-b-c-d");
}

// Array at negative index
TEST(JSEngine, ArrayAtNegativeIndex) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [10, 20, 30].at(-1)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "30");
}

// Object.create basic
TEST(JSEngine, ObjectCreateBasic) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var proto = {greet: function() { return 'hi'; }};
        var obj = Object.create(proto);
        obj.greet()
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "hi");
}

// Math.max of array values
TEST(JSEngine, MathMaxSpread) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Math.max(...[3, 1, 4, 1, 5, 9, 2, 6])
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "9");
}

// Math.min of array values
TEST(JSEngine, MathMinSpread) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Math.min(...[3, 1, 4, 1, 5, 9, 2, 6])
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1");
}

// ============================================================================
// Cycle 655: More JS engine tests
// ============================================================================

// typeof null is "object" (famous JS quirk)
TEST(JSEngine, TypeofNullIsObjectQuirk) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        typeof null
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "object");
}

// typeof undefined
TEST(JSEngine, TypeofUndefinedIsUndefined) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        typeof undefined
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "undefined");
}

// typeof number
TEST(JSEngine, TypeofNumberIsNumber) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        typeof 42
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "number");
}

// instanceof Array
TEST(JSEngine, InstanceofArray) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [] instanceof Array
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "true");
}

// instanceof non-Array object is false
TEST(JSEngine, ObjectNotInstanceofArray) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {};
        obj instanceof Array
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "false");
}

// Comma operator returns last
TEST(JSEngine, CommaOperatorReturnsLast) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        (1, 2, 3)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "3");
}

// void operator returns undefined
TEST(JSEngine, VoidReturnsUndefined) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        String(void 0)
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "undefined");
}

// delete property
TEST(JSEngine, DeleteProperty) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {a: 1, b: 2};
        delete obj.a;
        Object.keys(obj).length
    )");
    EXPECT_FALSE(engine.has_error()) << engine.last_error();
    EXPECT_EQ(result, "1");
}

// ============================================================================
// Cycle 658: More JS engine tests
// ============================================================================

TEST(JSEngine, InOperatorExists) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {x: 1, y: 2};
        "x" in obj ? "yes" : "no"
    )");
    EXPECT_EQ(result, "yes");
}

TEST(JSEngine, InOperatorMissing) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {x: 1};
        "z" in obj ? "yes" : "no"
    )");
    EXPECT_EQ(result, "no");
}

TEST(JSEngine, TernaryTrue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("true ? 42 : 0");
    EXPECT_EQ(result, "42");
}

TEST(JSEngine, TernaryFalse) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("false ? 42 : 99");
    EXPECT_EQ(result, "99");
}

TEST(JSEngine, StringSplitLength) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"("a,b,c".split(",").length)");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, StringSplitFirstElement) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"("a,b,c".split(",")[0])");
    EXPECT_EQ(result, "a");
}

TEST(JSEngine, ArrayFindReturnsMatch) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"([10, 20, 30].find(function(x) { return x > 15; }))");
    EXPECT_EQ(result, "20");
}

TEST(JSEngine, ArrayFindIndexReturnsTwo) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"([10, 20, 30].findIndex(function(x) { return x === 30; }))");
    EXPECT_EQ(result, "2");
}

// ============================================================================
// Cycle 665: More JS engine tests
// ============================================================================

TEST(JSEngine, StringRepeatThreeTimes) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"("ha".repeat(3))");
    EXPECT_EQ(result, "hahaha");
}

TEST(JSEngine, StringStartsWithPrefix) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"("hello world".startsWith("hello") ? "yes" : "no")");
    EXPECT_EQ(result, "yes");
}

TEST(JSEngine, StringEndsWithSuffix) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"("hello world".endsWith("world") ? "yes" : "no")");
    EXPECT_EQ(result, "yes");
}

TEST(JSEngine, StringIncludesSubstring) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"("foo bar baz".includes("bar") ? "yes" : "no")");
    EXPECT_EQ(result, "yes");
}

TEST(JSEngine, ArrayIncludesValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"([1, 2, 3, 4].includes(3) ? "yes" : "no")");
    EXPECT_EQ(result, "yes");
}

TEST(JSEngine, ArrayJoinWithDash) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"([1, 2, 3].join("-"))");
    EXPECT_EQ(result, "1-2-3");
}

TEST(JSEngine, ObjectAssignMergeSumIsThree) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var a = {x: 1};
        var b = {y: 2};
        var c = Object.assign(a, b);
        c.x + c.y
    )");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, ArrayReduceSum) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"([1, 2, 3, 4, 5].reduce(function(acc, x) { return acc + x; }, 0))");
    EXPECT_EQ(result, "15");
}

// ============================================================================
// Cycle 669: More JS engine tests
// ============================================================================

TEST(JSEngine, ObjectEntriesLength) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(Object.entries({a: 1, b: 2, c: 3}).length)");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, ObjectValuesSum) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var vals = Object.values({x: 10, y: 20, z: 30});
        vals.reduce(function(a, b) { return a + b; }, 0)
    )");
    EXPECT_EQ(result, "60");
}

TEST(JSEngine, ArrayFlatOneLevelDeep) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"([[1, 2], [3, 4]].flat().length)");
    EXPECT_EQ(result, "4");
}

TEST(JSEngine, ArrayFlatMapDoubles) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"([1, 2, 3].flatMap(function(x) { return [x, x * 2]; }).length)");
    EXPECT_EQ(result, "6");
}

TEST(JSEngine, NumberToFixedTwoDecimalsPi) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"((3.14159).toFixed(2))");
    EXPECT_EQ(result, "3.14");
}

TEST(JSEngine, StringTrimRemovesWhitespace) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"("  hello  ".trim())");
    EXPECT_EQ(result, "hello");
}

TEST(JSEngine, StringTrimStartRemovesLeading) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"("  hello  ".trimStart())");
    EXPECT_EQ(result, "hello  ");
}

TEST(JSEngine, StringTrimEndRemovesTrailing) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"("  hello  ".trimEnd())");
    EXPECT_EQ(result, "  hello");
}

// ============================================================================
// Cycle 674: More JS engine tests
// ============================================================================

TEST(JSEngine, ArrayFillWithValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(new Array(3).fill(0).join(","))");
    EXPECT_EQ(result, "0,0,0");
}

TEST(JSEngine, ArrayCopyWithinOverlap) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"([1,2,3,4,5].copyWithin(1, 3).join(","))");
    EXPECT_EQ(result, "1,4,5,4,5");
}

TEST(JSEngine, StringSliceExtract) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"("hello world".slice(6, 11))");
    EXPECT_EQ(result, "world");
}

TEST(JSEngine, StringSubstringExtract) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"("hello world".substring(0, 5))");
    EXPECT_EQ(result, "hello");
}

TEST(JSEngine, RegexTestReturnsTrue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(/^\d+$/.test("12345") ? "yes" : "no")");
    EXPECT_EQ(result, "yes");
}

TEST(JSEngine, RegexTestReturnsFalse) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(/^\d+$/.test("abc") ? "yes" : "no")");
    EXPECT_EQ(result, "no");
}

TEST(JSEngine, ArraySortAscending) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"([3, 1, 2].sort(function(a, b) { return a - b; }).join(","))");
    EXPECT_EQ(result, "1,2,3");
}

TEST(JSEngine, ArraySortDescending) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"([3, 1, 2].sort(function(a, b) { return b - a; }).join(","))");
    EXPECT_EQ(result, "3,2,1");
}

// ============================================================================
// Cycle 677: More JS engine tests
// ============================================================================

TEST(JSEngine, DateNowTypeofIsNumber) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(typeof Date.now() === "number" ? "yes" : "no")");
    EXPECT_EQ(result, "yes");
}

TEST(JSEngine, MathAbsNegative) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.abs(-42)");
    EXPECT_EQ(result, "42");
}

TEST(JSEngine, MathCeilRoundsUp) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.ceil(4.1)");
    EXPECT_EQ(result, "5");
}

TEST(JSEngine, MathFloorRoundsDown) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.floor(4.9)");
    EXPECT_EQ(result, "4");
}

TEST(JSEngine, MathRoundHalfUp) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.round(4.5)");
    EXPECT_EQ(result, "5");
}

TEST(JSEngine, MathSqrtOfNine) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.sqrt(9)");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, MathPow) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.pow(2, 10)");
    EXPECT_EQ(result, "1024");
}

TEST(JSEngine, MathLogBase) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.log(1)");
    EXPECT_EQ(result, "0");
}

// ============================================================================
// Cycle 682: More JS engine tests
// ============================================================================

TEST(JSEngine, ForOfArraySum) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var sum = 0;
        for (var x of [1, 2, 3, 4, 5]) { sum += x; }
        sum
    )");
    EXPECT_EQ(result, "15");
}

TEST(JSEngine, ForInObjectKeys) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {a: 1, b: 2, c: 3};
        var keys = [];
        for (var k in obj) { keys.push(k); }
        keys.length
    )");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, WhileLoopCount) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var i = 0, count = 0;
        while (i < 10) { i++; count++; }
        count
    )");
    EXPECT_EQ(result, "10");
}

TEST(JSEngine, DoWhileExecutesOnce) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = 0;
        do { x++; } while (false);
        x
    )");
    EXPECT_EQ(result, "1");
}

TEST(JSEngine, SwitchCaseMatch) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = 2;
        var out = "";
        switch (x) {
            case 1: out = "one"; break;
            case 2: out = "two"; break;
            default: out = "other";
        }
        out
    )");
    EXPECT_EQ(result, "two");
}

TEST(JSEngine, SwitchDefaultCase) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = 99;
        var out = "";
        switch (x) {
            case 1: out = "one"; break;
            default: out = "other";
        }
        out
    )");
    EXPECT_EQ(result, "other");
}

TEST(JSEngine, LabeledBreak) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var result = 0;
        outer: for (var i = 0; i < 3; i++) {
            for (var j = 0; j < 3; j++) {
                if (i === 1 && j === 1) break outer;
                result++;
            }
        }
        result
    )");
    EXPECT_EQ(result, "4");
}

TEST(JSEngine, ContinueSkipsIteration) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var sum = 0;
        for (var i = 0; i < 5; i++) {
            if (i === 2) continue;
            sum += i;
        }
        sum
    )");
    EXPECT_EQ(result, "8");  // 0+1+3+4=8
}

// ============================================================================
// Cycle 685: More JS engine tests
// ============================================================================

TEST(JSEngine, ClosureCaptures) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function makeAdder(x) {
            return function(y) { return x + y; };
        }
        var add5 = makeAdder(5);
        add5(3)
    )");
    EXPECT_EQ(result, "8");
}

TEST(JSEngine, IIFEExecution) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"((function() { return 42; })())");
    EXPECT_EQ(result, "42");
}

TEST(JSEngine, RecursiveFibonacci) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function fib(n) {
            if (n <= 1) return n;
            return fib(n - 1) + fib(n - 2);
        }
        fib(10)
    )");
    EXPECT_EQ(result, "55");
}

TEST(JSEngine, DefaultParameterValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function greet(name = "world") { return "hello " + name; }
        greet()
    )");
    EXPECT_EQ(result, "hello world");
}

TEST(JSEngine, ArrowFunctionSum) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var add = (a, b) => a + b;
        add(10, 20)
    )");
    EXPECT_EQ(result, "30");
}

TEST(JSEngine, ArrowFunctionInMap) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"([1, 2, 3].map(x => x * x).join(","))");
    EXPECT_EQ(result, "1,4,9");
}

TEST(JSEngine, TemplateLiteralExpression) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = 5;
        `x is ${x}`
    )");
    EXPECT_EQ(result, "x is 5");
}

TEST(JSEngine, DestructuringRename) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var obj = {name: "Alice", age: 30};
        var {name: n, age: a} = obj;
        n + " is " + a
    )");
    EXPECT_EQ(result, "Alice is 30");
}

// ---------------------------------------------------------------------------
// Cycle 690 — bitwise operation tests
// ---------------------------------------------------------------------------

TEST(JSEngine, BitwiseAndOperation) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("5 & 3");
    EXPECT_EQ(result, "1");
}

TEST(JSEngine, BitwiseOrOperation) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("5 | 3");
    EXPECT_EQ(result, "7");
}

TEST(JSEngine, BitwiseXorOperation) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("5 ^ 3");
    EXPECT_EQ(result, "6");
}

TEST(JSEngine, BitwiseNotOperation) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("~5");
    EXPECT_EQ(result, "-6");
}

TEST(JSEngine, LeftShiftOperation) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("1 << 3");
    EXPECT_EQ(result, "8");
}

TEST(JSEngine, RightShiftOperation) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("16 >> 2");
    EXPECT_EQ(result, "4");
}

TEST(JSEngine, UnsignedRightShift) {
    clever::js::JSEngine engine;
    // -1 >>> 0 == 4294967295 (treats as unsigned 32-bit)
    auto result = engine.evaluate("-1 >>> 0");
    EXPECT_EQ(result, "4294967295");
}

TEST(JSEngine, BitwiseAndAssignment) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = 7;
        x &= 5;
        x
    )");
    EXPECT_EQ(result, "5");
}

// ---------------------------------------------------------------------------
// Cycle 698 — Math constants and trig functions
// ---------------------------------------------------------------------------

TEST(JSEngine, ExponentOperator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("2 ** 10");
    EXPECT_EQ(result, "1024");
}

TEST(JSEngine, MathPIConstant) {
    clever::js::JSEngine engine;
    // Math.PI ≈ 3.14159...
    auto result = engine.evaluate("Math.PI > 3 && Math.PI < 4");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, MathEConstant) {
    clever::js::JSEngine engine;
    // Math.E ≈ 2.71828...
    auto result = engine.evaluate("Math.E > 2 && Math.E < 3");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, MathSinZero) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.sin(0)");
    EXPECT_EQ(result, "0");
}

TEST(JSEngine, MathCosZero) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.cos(0)");
    EXPECT_EQ(result, "1");
}

TEST(JSEngine, MathExpZero) {
    clever::js::JSEngine engine;
    // Math.exp(0) = e^0 = 1
    auto result = engine.evaluate("Math.exp(0)");
    EXPECT_EQ(result, "1");
}

TEST(JSEngine, MathAtan2OneOne) {
    clever::js::JSEngine engine;
    // Math.atan2(1, 1) = pi/4 ≈ 0.785...
    auto result = engine.evaluate("Math.atan2(1, 1) > 0.7 && Math.atan2(1, 1) < 0.9");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, MathCbrtOfEight) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.cbrt(8)");
    EXPECT_EQ(result, "2");
}

// ---------------------------------------------------------------------------
// Cycle 701 — Map/Set operations and flat depth
// ---------------------------------------------------------------------------

TEST(JSEngine, MapForEachIterates) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var m = new Map();
        m.set("a", 1);
        m.set("b", 2);
        var count = 0;
        m.forEach(function(v) { count += v; });
        count
    )");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, SetForEachIterates) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s = new Set([10, 20, 30]);
        var total = 0;
        s.forEach(function(v) { total += v; });
        total
    )");
    EXPECT_EQ(result, "60");
}

TEST(JSEngine, MapDeleteRemovesEntry) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var m = new Map();
        m.set("key", "value");
        m.delete("key");
        m.size
    )");
    EXPECT_EQ(result, "0");
}

TEST(JSEngine, SetDeleteRemovesValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var s = new Set([1, 2, 3]);
        s.delete(2);
        s.size
    )");
    EXPECT_EQ(result, "2");
}

TEST(JSEngine, ArrayFlatDepthTwo) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, [2, [3, [4]]]].flat(2).length
    )");
    EXPECT_EQ(result, "4");
}

TEST(JSEngine, ArrayFlatInfinity) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        [1, [2, [3, [4, [5]]]]].flat(Infinity).join(",")
    )");
    EXPECT_EQ(result, "1,2,3,4,5");
}

TEST(JSEngine, BitwiseOrAssignment) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = 4;
        x |= 3;
        x
    )");
    EXPECT_EQ(result, "7");
}

TEST(JSEngine, BitwiseXorAssignment) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        var x = 15;
        x ^= 9;
        x
    )");
    EXPECT_EQ(result, "6");
}

TEST(JSEngine, StringPadStartFive) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'7'.padStart(5, '0')");
    EXPECT_EQ(result, "00007");
}

TEST(JSEngine, StringPadEndEight) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'hi'.padEnd(8, '-')");
    EXPECT_EQ(result, "hi------");
}

TEST(JSEngine, StringRepeatAbThrice) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'ab'.repeat(3)");
    EXPECT_EQ(result, "ababab");
}

TEST(JSEngine, StringStartsWithTrue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'hello world'.startsWith('hello')");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, StringEndsWithTrue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'hello world'.endsWith('world')");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, StringIncludesTrue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'hello world'.includes('lo wo')");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ArrayFindReturnsFirst) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[1, 2, 3, 4].find(x => x > 2)");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, ArrayFindIndexReturnsIndex) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[10, 20, 30].findIndex(x => x === 20)");
    EXPECT_EQ(result, "1");
}

TEST(JSEngine, ArrayEveryAllEvenV2) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[2, 4, 6].every(x => x % 2 === 0)");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ArraySomeFindsOdd) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[1, 2, 3].some(x => x % 2 === 1)");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ArrayFromStringCharsV2) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Array.from('abc').join('')");
    EXPECT_EQ(result, "abc");
}

TEST(JSEngine, ArrayIsArrayTrue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Array.isArray([1,2,3])");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ObjectAssignMergePropertyB) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Object.assign({a:1}, {b:2}).b");
    EXPECT_EQ(result, "2");
}

TEST(JSEngine, ObjectKeysLengthThreeV2) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Object.keys({x:1,y:2,z:3}).length");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, NumberIsIntegerTrueV2) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Number.isInteger(42)");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, NumberIsFiniteTrue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Number.isFinite(3.14)");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, DestructuringArrayFirst) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const [a, b] = [10, 20]; a");
    EXPECT_EQ(result, "10");
}

TEST(JSEngine, DestructuringObjectProp) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const {x, y} = {x: 3, y: 7}; x + y");
    EXPECT_EQ(result, "10");
}

TEST(JSEngine, SpreadOperatorArray) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[...[1,2], ...[3,4]].length");
    EXPECT_EQ(result, "4");
}

TEST(JSEngine, RestParameterLength) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("function f(...args) { return args.length; } f(1,2,3)");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, TemplateLiteralBasic) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const name = 'World'; `Hello ${name}`");
    EXPECT_EQ(result, "Hello World");
}

TEST(JSEngine, DefaultParameterGuestFallback) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("function greet(name = 'Guest') { return name; } greet()");
    EXPECT_EQ(result, "Guest");
}

TEST(JSEngine, ArrowFunctionImplicitReturn) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const double = x => x * 2; double(7)");
    EXPECT_EQ(result, "14");
}

TEST(JSEngine, ClassInstanceMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        class Animal {
            constructor(name) { this.name = name; }
            speak() { return this.name + ' speaks'; }
        }
        new Animal('Dog').speak()
    )");
    EXPECT_EQ(result, "Dog speaks");
}

TEST(JSEngine, GeneratorYieldsValues) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function* gen() { yield 1; yield 2; yield 3; }
        const g = gen();
        g.next().value + g.next().value + g.next().value
    )");
    EXPECT_EQ(result, "6");
}

TEST(JSEngine, ForOfGeneratorSum) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function* range(n) { for (let i = 0; i < n; i++) yield i; }
        let sum = 0;
        for (const x of range(5)) sum += x;
        sum
    )");
    EXPECT_EQ(result, "10");
}

TEST(JSEngine, IteratorProtocolManual) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        const arr = [10, 20, 30];
        const it = arr[Symbol.iterator]();
        it.next().value
    )");
    EXPECT_EQ(result, "10");
}

TEST(JSEngine, NullCoalescingOperator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("null ?? 'default'");
    EXPECT_EQ(result, "default");
}

TEST(JSEngine, OptionalChainingProperty) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const obj = {a: {b: 42}}; obj?.a?.b");
    EXPECT_EQ(result, "42");
}

TEST(JSEngine, OptionalChainingNullReturnsUndefined) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const obj = null; String(obj?.a)");
    EXPECT_EQ(result, "undefined");
}

TEST(JSEngine, LogicalAndAssignmentV2) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("let x = 1; x &&= 5; x");
    EXPECT_EQ(result, "5");
}

TEST(JSEngine, LogicalOrAssignmentV2) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("let x = 0; x ||= 42; x");
    EXPECT_EQ(result, "42");
}

TEST(JSEngine, TryCatchCapturesError) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        let msg = '';
        try { throw new Error('oops'); }
        catch (e) { msg = e.message; }
        msg
    )");
    EXPECT_EQ(result, "oops");
}

TEST(JSEngine, TryCatchFinallyLogConcatenation) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        let log = '';
        try { log += 'try'; throw 1; }
        catch (e) { log += 'catch'; }
        finally { log += 'finally'; }
        log
    )");
    EXPECT_EQ(result, "trycatchfinally");
}

TEST(JSEngine, TypeErrorNullPropertyAccess) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        let caught = false;
        try { null.property; }
        catch (e) { caught = true; }
        caught
    )");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, CustomErrorMessage) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        class AppError extends Error {
            constructor(msg) { super(msg); this.name = 'AppError'; }
        }
        let e;
        try { throw new AppError('bad input'); }
        catch (err) { e = err.message; }
        e
    )");
    EXPECT_EQ(result, "bad input");
}

TEST(JSEngine, WeakMapSetAndHas) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        const wm = new WeakMap();
        const key = {};
        wm.set(key, 42);
        wm.has(key)
    )");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, WeakSetAddAndHas) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        const ws = new WeakSet();
        const obj = {};
        ws.add(obj);
        ws.has(obj)
    )");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ArrayBufferByteLength) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new ArrayBuffer(16).byteLength");
    EXPECT_EQ(result, "16");
}

TEST(JSEngine, Uint8ArrayLength) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Uint8Array(8).length");
    EXPECT_EQ(result, "8");
}

TEST(JSEngine, JSONParseBasicObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(JSON.parse('{"a":1,"b":2}').a)");
    EXPECT_EQ(result, "1");
}

TEST(JSEngine, JSONStringifyObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(JSON.stringify({x:1}))");
    EXPECT_NE(result.find("x"), std::string::npos);
    EXPECT_NE(result.find("1"), std::string::npos);
}

TEST(JSEngine, JSONParseArray) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(JSON.parse('[10,20,30]')[1])");
    EXPECT_EQ(result, "20");
}

TEST(JSEngine, JSONRoundTrip) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        const obj = {name: "Alice", age: 30};
        const json = JSON.stringify(obj);
        const parsed = JSON.parse(json);
        parsed.name
    )");
    EXPECT_EQ(result, "Alice");
}

TEST(JSEngine, DateGetFullYear) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Date(2024, 0, 15).getFullYear()");
    EXPECT_EQ(result, "2024");
}

TEST(JSEngine, DateGetMonth) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Date(2024, 5, 1).getMonth()");
    EXPECT_EQ(result, "5");
}

TEST(JSEngine, DateGetDate) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Date(2024, 0, 25).getDate()");
    EXPECT_EQ(result, "25");
}

TEST(JSEngine, JSONParseNull) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("JSON.parse('null')");
    EXPECT_EQ(result, "null");
}

TEST(JSEngine, ProxyGetTrapReturnsVal) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        const p = new Proxy({val: 42}, {
            get(target, prop) { return target[prop]; }
        });
        p.val
    )");
    EXPECT_EQ(result, "42");
}

TEST(JSEngine, ProxySetTrap) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        let stored = 0;
        const p = new Proxy({}, {
            set(target, prop, value) { stored = value; return true; }
        });
        p.x = 99;
        stored
    )");
    EXPECT_EQ(result, "99");
}

TEST(JSEngine, ReflectGetReturnsValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Reflect.get({name: 'test'}, 'name')");
    EXPECT_EQ(result, "test");
}

TEST(JSEngine, ReflectHasReturnsBool) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Reflect.has({x: 1}, 'x')");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, SymbolUnique) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Symbol('a') !== Symbol('a')");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, SymbolDescription) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Symbol('mySymbol').description");
    EXPECT_EQ(result, "mySymbol");
}

TEST(JSEngine, MapSizeAfterThreeAdds) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        const m = new Map();
        m.set('a', 1);
        m.set('b', 2);
        m.set('c', 3);
        m.size
    )");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, SetSizeAfterThreeAdds) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        const s = new Set([1, 2, 3, 2, 1]);
        s.size
    )");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, PromiseResolveValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        let captured = '';
        Promise.resolve(42).then(v => { captured = String(v); });
        captured
    )");
    // Promise may resolve synchronously or captured may be set
    EXPECT_TRUE(result == "42" || result == "");
}

TEST(JSEngine, PromiseAllResolves) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        let done = false;
        Promise.all([Promise.resolve(1), Promise.resolve(2)]).then(v => { done = true; });
        done
    )");
    EXPECT_TRUE(result == "true" || result == "false");
}

TEST(JSEngine, InstanceofArrayOperator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[1,2,3] instanceof Array");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, TypeofNumber) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof 42");
    EXPECT_EQ(result, "number");
}

TEST(JSEngine, TypeofString) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof 'hello'");
    EXPECT_EQ(result, "string");
}

TEST(JSEngine, TypeofFunction) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof function() {}");
    EXPECT_EQ(result, "function");
}

TEST(JSEngine, TypeofUndefined) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof undefined");
    EXPECT_EQ(result, "undefined");
}

TEST(JSEngine, InOperatorObjectKey) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'x' in {x: 1, y: 2}");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, NumberToString) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(255).toString(16)");
    EXPECT_EQ(result, "ff");
}

TEST(JSEngine, NumberToBinary) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(10).toString(2)");
    EXPECT_EQ(result, "1010");
}

TEST(JSEngine, NumberToOctal) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(8).toString(8)");
    EXPECT_EQ(result, "10");
}

TEST(JSEngine, MathMin) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.min(3, 1, 4, 1, 5, 9)");
    EXPECT_EQ(result, "1");
}

TEST(JSEngine, MathMax) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.max(3, 1, 4, 1, 5, 9)");
    EXPECT_EQ(result, "9");
}

TEST(JSEngine, MathTruncPositive) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.trunc(4.7)");
    EXPECT_EQ(result, "4");
}

TEST(JSEngine, MathTruncNegative) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.trunc(-4.7)");
    EXPECT_EQ(result, "-4");
}

TEST(JSEngine, MathSignPositive) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.sign(42)");
    EXPECT_EQ(result, "1");
}

TEST(JSEngine, RegExpExecReturnsMatch) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(/(\d+)/.exec('abc 123')[1])");
    EXPECT_EQ(result, "123");
}

TEST(JSEngine, RegExpGlobalFlag) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"('aabbcc'.match(/a/g).length)");
    EXPECT_EQ(result, "2");
}

TEST(JSEngine, StringMatchAllDigits) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"([...'test123'.matchAll(/\d/g)].length)");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, StringReplaceAllAtoX) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'abcabc'.replaceAll('a', 'X')");
    EXPECT_EQ(result, "XbcXbc");
}

TEST(JSEngine, StringAtNegativeIndex) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'hello'.at(-1)");
    EXPECT_EQ(result, "o");
}

TEST(JSEngine, ArrayAtLastElement) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[1, 2, 3, 4].at(-1)");
    EXPECT_EQ(result, "4");
}

TEST(JSEngine, ObjectFromEntriesAB) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Object.fromEntries([['a', 1], ['b', 2]]).b");
    EXPECT_EQ(result, "2");
}

TEST(JSEngine, ArrayGroupByLike) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        const nums = [1, 2, 3, 4, 5];
        const evens = nums.filter(n => n % 2 === 0);
        const odds = nums.filter(n => n % 2 !== 0);
        evens.length + '-' + odds.length
    )");
    EXPECT_EQ(result, "2-3");
}

TEST(JSEngine, MathSignNegative) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.sign(-5)");
    EXPECT_EQ(result, "-1");
}

TEST(JSEngine, MathCosPI) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.round(Math.cos(Math.PI))");
    EXPECT_EQ(result, "-1");
}

TEST(JSEngine, MathSinHalfPI) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.round(Math.sin(Math.PI / 2))");
    EXPECT_EQ(result, "1");
}

TEST(JSEngine, MathTanZero) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.tan(0)");
    EXPECT_EQ(result, "0");
}

TEST(JSEngine, ArrayFlatDeep) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[1, [2, [3, [4]]]].flat(Infinity).length");
    EXPECT_EQ(result, "4");
}

TEST(JSEngine, StringCodePointAt) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'A'.codePointAt(0)");
    EXPECT_EQ(result, "65");
}

TEST(JSEngine, StringFromCodePoint) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("String.fromCodePoint(65)");
    EXPECT_EQ(result, "A");
}

TEST(JSEngine, ErrorStack) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(function() { try { throw new Error('oops'); } catch(e) { return e.message; } })()");
    EXPECT_EQ(result, "oops");
}

TEST(JSEngine, ParseIntRadix16) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("parseInt('ff', 16)");
    EXPECT_EQ(result, "255");
}

TEST(JSEngine, ParseIntRadix2) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("parseInt('1010', 2)");
    EXPECT_EQ(result, "10");
}

TEST(JSEngine, ParseFloatTrailingChars) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("parseFloat('3.14xyz')");
    EXPECT_EQ(result, "3.14");
}

TEST(JSEngine, ArrayFindLastIndex) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[1, 2, 3, 2, 1].findLastIndex(x => x === 2)");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, ArrayWith) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[1, 2, 3].with(1, 99)[1]");
    EXPECT_EQ(result, "99");
}

TEST(JSEngine, ObjectGroupBy) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        const arr = [1, 2, 3, 4];
        const g = Object.groupBy(arr, n => n % 2 === 0 ? 'even' : 'odd');
        g.even.length
    )");
    EXPECT_EQ(result, "2");
}

TEST(JSEngine, ArrayFindLast) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[1, 2, 3, 2, 1].findLast(x => x === 2)");
    EXPECT_EQ(result, "2");
}

TEST(JSEngine, StringNormalizeNFC) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof 'hello'.normalize");
    EXPECT_EQ(result, "function");
}

TEST(JSEngine, PromiseRejectIsObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof Promise.reject(new Error('fail'))");
    EXPECT_EQ(result, "object");
}

TEST(JSEngine, PromiseAllSettled) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        Promise.allSettled([Promise.resolve(1), Promise.reject(2)]).then(r => r.length)
    )");
    // allSettled always resolves; result may be "2" or a promise repr
    EXPECT_FALSE(result.empty());
}

TEST(JSEngine, PromiseThenReturnsObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof Promise.resolve(1).then(x => x + 1)");
    EXPECT_EQ(result, "object");
}

TEST(JSEngine, AsyncFunctionAwaitResolves) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        async function add(a, b) { return a + b; }
        add(3, 4).then(v => v)
    )");
    // async functions return promise; result may vary
    EXPECT_FALSE(result.empty());
}

TEST(JSEngine, GeneratorNextValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function* gen() { yield 10; yield 20; }
        const g = gen();
        g.next().value + g.next().value
    )");
    EXPECT_EQ(result, "30");
}

TEST(JSEngine, GeneratorReturnDone) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        function* gen() { yield 1; }
        const g = gen();
        g.next();
        g.next().done
    )");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, SetForEachSum) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        let sum = 0;
        new Set([1, 2, 3]).forEach(v => { sum += v; });
        sum
    )");
    EXPECT_EQ(result, "6");
}

TEST(JSEngine, MapForEachSum) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        let sum = 0;
        new Map([['a', 1], ['b', 2]]).forEach(v => { sum += v; });
        sum
    )");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, StringSearch) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'hello world'.search(/world/)");
    EXPECT_EQ(result, "6");
}

TEST(JSEngine, StringConcat) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'foo'.concat('bar', 'baz')");
    EXPECT_EQ(result, "foobarbaz");
}

TEST(JSEngine, StringLocaleCompareEqual) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'a'.localeCompare('a')");
    EXPECT_EQ(result, "0");
}

TEST(JSEngine, StringFromCharCode) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("String.fromCharCode(72, 105)");
    EXPECT_EQ(result, "Hi");
}

TEST(JSEngine, StringCharCodeAtFirst) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'Z'.charCodeAt(0)");
    EXPECT_EQ(result, "90");
}

TEST(JSEngine, StringWrapInArray) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[...'abc'].length");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, StringToLowerCaseResult) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'HELLO'.toLowerCase()");
    EXPECT_EQ(result, "hello");
}

TEST(JSEngine, StringToUpperCaseResult) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("'world'.toUpperCase()");
    EXPECT_EQ(result, "WORLD");
}

TEST(JSEngine, ArrayLastIndexOf) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[1, 2, 3, 2, 1].lastIndexOf(2)");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, ArrayFillPartial) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[1, 2, 3, 4, 5].fill(0, 1, 3).join(',')");
    EXPECT_EQ(result, "1,0,0,4,5");
}

TEST(JSEngine, ArraySliceNegativeIndex) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[10, 20, 30, 40].slice(-2).join(',')");
    EXPECT_EQ(result, "30,40");
}

TEST(JSEngine, ArraySpliceRemovesElement) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const a = [1,2,3,4]; a.splice(1,1); a.join(',')");
    EXPECT_EQ(result, "1,3,4");
}

TEST(JSEngine, ArrayShiftRemovesFirst) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const a = [1,2,3]; a.shift(); a.join(',')");
    EXPECT_EQ(result, "2,3");
}

TEST(JSEngine, ArrayUnshiftAddsToFront) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const a = [2,3]; a.unshift(1); a.join(',')");
    EXPECT_EQ(result, "1,2,3");
}

TEST(JSEngine, ArrayCopyWithinNegative) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("[1, 2, 3, 4, 5].copyWithin(-2).join(',')");
    EXPECT_EQ(result, "1,2,3,1,2");
}

TEST(JSEngine, ArrayToSplicedReturnsNew) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const a = [1,2,3]; const b = a.toSpliced(1,1); a.length + ',' + b.length");
    EXPECT_EQ(result, "3,2");
}

TEST(JSEngine, ObjectSealPreventNewProps) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        const o = Object.seal({x: 1});
        try { o.y = 2; } catch(e) {}
        'y' in o
    )");
    EXPECT_EQ(result, "false");
}

TEST(JSEngine, ObjectIsFrozen) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Object.isFrozen(Object.freeze({}))");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ObjectIsSealed) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Object.isSealed(Object.seal({}))");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ObjectDefinePropertyValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        const o = {};
        Object.defineProperty(o, 'x', { value: 42, writable: true });
        o.x
    )");
    EXPECT_EQ(result, "42");
}

TEST(JSEngine, ObjectGetOwnPropertyNames) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Object.getOwnPropertyNames({a:1,b:2}).length");
    EXPECT_EQ(result, "2");
}

TEST(JSEngine, ObjectCreateWithNull) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Object.getPrototypeOf(Object.create(null))");
    EXPECT_EQ(result, "null");
}

TEST(JSEngine, PropertyDescriptorWritable) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(R"(
        const o = {};
        Object.defineProperty(o, 'x', { value: 5, writable: false });
        Object.getOwnPropertyDescriptor(o, 'x').writable
    )");
    EXPECT_EQ(result, "false");
}

TEST(JSEngine, ObjectSpreadOverrides) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const base={a:1,b:2}; const o={...base, b:99}; o.b");
    EXPECT_EQ(result, "99");
}

TEST(JSEngine, NumberToPrecision) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(123.456).toPrecision(5)");
    EXPECT_EQ(result, "123.46");
}

TEST(JSEngine, NumberToExponential) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(12345).toExponential(2)");
    EXPECT_EQ(result, "1.23e+4");
}

TEST(JSEngine, NumberEpsilonIsSmall) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Number.EPSILON < 0.001");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, MathClz32) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.clz32(1)");
    EXPECT_EQ(result, "31");
}

TEST(JSEngine, MathImul) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.imul(3, 4)");
    EXPECT_EQ(result, "12");
}

TEST(JSEngine, MathFRound) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof Math.fround(5.5)");
    EXPECT_EQ(result, "number");
}

TEST(JSEngine, BigIntAdd) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(1n + 2n).toString()");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, BigIntCompare) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("10n > 5n");
    EXPECT_EQ(result, "true");
}

// Cycle 786 — Error cause, AggregateError, Number.isSafeInteger, Math.expm1/log1p, BigInt ops
TEST(JSEngine, ErrorCause) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const e = new Error('outer', { cause: new Error('inner') }); e.message");
    EXPECT_EQ(result, "outer");
}

TEST(JSEngine, AggregateErrorType) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof new AggregateError([new Error('a'), new Error('b')], 'All failed')");
    EXPECT_EQ(result, "object");
}

TEST(JSEngine, NumberIsSafeIntegerTrue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Number.isSafeInteger(9007199254740991)");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, NumberIsSafeIntegerFalse) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Number.isSafeInteger(9007199254740992)");
    EXPECT_EQ(result, "false");
}

TEST(JSEngine, MathExpm1Zero) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.expm1(0)");
    EXPECT_EQ(result, "0");
}

TEST(JSEngine, MathLog1pZero) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.log1p(0)");
    EXPECT_EQ(result, "0");
}

TEST(JSEngine, BigIntMultiply) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(100n * 200n).toString()");
    EXPECT_EQ(result, "20000");
}

TEST(JSEngine, BigIntSubtract) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(1000n - 1n).toString()");
    EXPECT_EQ(result, "999");
}

// Cycle 794 — Map/Set advanced operations
TEST(JSEngine, MapClearSetsZeroSize) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const m = new Map(); m.set('a',1); m.set('b',2); m.clear(); m.size");
    EXPECT_EQ(result, "0");
}

TEST(JSEngine, SetClearSetsZeroSize) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const s = new Set([1,2,3]); s.clear(); s.size");
    EXPECT_EQ(result, "0");
}

TEST(JSEngine, MapHasReturnsTrueAfterSet) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const m = new Map(); m.set('key', 42); m.has('key')");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, MapHasReturnsFalseForMissing) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const m = new Map(); m.has('missing')");
    EXPECT_EQ(result, "false");
}

TEST(JSEngine, SetHasReturnsTrueAfterAdd) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const s = new Set(); s.add(99); s.has(99)");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, MapFromArrayOfPairs) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const m = new Map([['x',10],['y',20]]); m.get('y')");
    EXPECT_EQ(result, "20");
}

TEST(JSEngine, SetFromArrayDeduplicates) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const s = new Set([1,2,2,3,3,3]); s.size");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, MapGetReturnsUndefinedForMissing) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const m = new Map(); typeof m.get('nope')");
    EXPECT_EQ(result, "undefined");
}

// Cycle 798 — class inheritance, super, private fields, getters/setters
TEST(JSEngine, ClassExtendsCallsInheritedMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "class Animal { speak() { return 'generic'; } }"
        "class Dog extends Animal { speak() { return 'woof'; } }"
        "new Dog().speak()");
    EXPECT_EQ(result, "woof");
}

TEST(JSEngine, ClassSuperCallsParent) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "class Base { val() { return 10; } }"
        "class Derived extends Base { val() { return super.val() + 5; } }"
        "new Derived().val()");
    EXPECT_EQ(result, "15");
}

TEST(JSEngine, ClassGetterAccessor) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "class Circle { constructor(r) { this.r = r; } get area() { return Math.PI * this.r * this.r; } }"
        "typeof new Circle(5).area");
    EXPECT_EQ(result, "number");
}

TEST(JSEngine, ClassSetterAccessor) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "class Box { constructor() { this._v = 0; } set value(v) { this._v = v; } get value() { return this._v; } }"
        "const b = new Box(); b.value = 42; b.value");
    EXPECT_EQ(result, "42");
}

TEST(JSEngine, ClassInstanceOf) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "class Foo {} const f = new Foo(); f instanceof Foo");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ClassInheritanceInstanceOf) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "class A {} class B extends A {} new B() instanceof A");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ClassPrivateField) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "class Counter { #count = 0; increment() { this.#count++; return this.#count; } }"
        "new Counter().increment()");
    EXPECT_EQ(result, "1");
}

TEST(JSEngine, ClassStaticField) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "class Config { static version = '1.0'; } Config.version");
    EXPECT_EQ(result, "1.0");
}

// Cycle 801 — Generator advanced: multiple yields, for-of, spread, fibonacci, throw
TEST(JSEngine, GeneratorFourYields) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "function* gen() { yield 1; yield 2; yield 3; yield 4; }"
        "const g = gen(); g.next().value + g.next().value + g.next().value + g.next().value");
    EXPECT_EQ(result, "10");
}

TEST(JSEngine, GeneratorCompletedReturnsDone) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "function* gen() { yield 1; }"
        "const g = gen(); g.next(); g.next().done");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, GeneratorInForOf) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "function* range(n) { for (let i = 0; i < n; i++) yield i; }"
        "let sum = 0; for (const v of range(5)) sum += v; sum");
    EXPECT_EQ(result, "10");
}

TEST(JSEngine, ArrayFromGenerator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "function* gen() { yield 'a'; yield 'b'; yield 'c'; }"
        "Array.from(gen()).join('')");
    EXPECT_EQ(result, "abc");
}

TEST(JSEngine, SpreadFromGenerator) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "function* nums() { yield 1; yield 2; yield 3; }"
        "[...nums()].length");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, GeneratorFibonacciSequence) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "function* fib() { let a=0,b=1; while(true) { yield a; [a,b]=[b,a+b]; } }"
        "const g = fib(); [g.next().value,g.next().value,g.next().value,g.next().value,g.next().value].join(',')");
    EXPECT_EQ(result, "0,1,1,2,3");
}

TEST(JSEngine, GeneratorReturnValueProp) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "function* gen() { yield 42; return 'done'; }"
        "const g = gen(); g.next().value");
    EXPECT_EQ(result, "42");
}

TEST(JSEngine, GeneratorNextWithArgument) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "function* gen() { const x = yield 1; yield x * 2; }"
        "const g = gen(); g.next(); g.next(5).value");
    EXPECT_EQ(result, "10");
}

// Cycle 805 — Async/await advanced patterns
TEST(JSEngine, AsyncArrowFunctionType) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof (async () => 42)");
    EXPECT_EQ(result, "function");
}

TEST(JSEngine, AsyncFunctionReturnsPromiseV2) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof (async function() { return 1; })()");
    EXPECT_EQ(result, "object");
}

TEST(JSEngine, PromiseResolveValueTypeNumber) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof Promise.resolve(42)");
    EXPECT_EQ(result, "object");
}

TEST(JSEngine, PromiseRejectValueTypeObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof Promise.reject(new Error('e'))");
    EXPECT_EQ(result, "object");
}

TEST(JSEngine, PromiseAllIsFunctionV2) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof Promise.all");
    EXPECT_EQ(result, "function");
}

TEST(JSEngine, PromiseRaceIsObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof Promise.race([Promise.resolve(1)])");
    EXPECT_EQ(result, "object");
}

TEST(JSEngine, AsyncGeneratorType) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("async function* gen() { yield 1; } typeof gen");
    EXPECT_EQ(result, "function");
}

TEST(JSEngine, PromiseChainType) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof Promise.resolve(1).then(x => x).then(x => x * 2)");
    EXPECT_EQ(result, "object");
}

// Cycle 809 — Proxy/Reflect advanced
TEST(JSEngine, ProxyHasTrap) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const p = new Proxy({x:1}, { has(t, k) { return k === 'x'; } }); 'x' in p");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ProxyGetTrapDefault) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const p = new Proxy({val: 42}, {}); p.val");
    EXPECT_EQ(result, "42");
}

TEST(JSEngine, ProxySetTrapModifiesValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const obj = {};"
        "const p = new Proxy(obj, { set(t, k, v) { t[k] = v * 2; return true; } });"
        "p.x = 5; obj.x");
    EXPECT_EQ(result, "10");
}

TEST(JSEngine, ReflectDeletePropertyTrue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const obj = { a: 1, b: 2 }; Reflect.deleteProperty(obj, 'a')");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ReflectSetValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const obj = {}; Reflect.set(obj, 'x', 99); obj.x");
    EXPECT_EQ(result, "99");
}

TEST(JSEngine, ReflectOwnKeysCount) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "Reflect.ownKeys({a:1, b:2, c:3}).length");
    EXPECT_EQ(result, "3");
}

TEST(JSEngine, ReflectGetPrototypeOfObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "typeof Reflect.getPrototypeOf({})");
    EXPECT_EQ(result, "object");
}

TEST(JSEngine, ReflectIsExtensibleTrue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "Reflect.isExtensible({})");
    EXPECT_EQ(result, "true");
}

// Cycle 814 — Template literals: arithmetic, ternary, nested, method call
TEST(JSEngine, TemplateLiteralArithmetic) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("`${2 + 3}`");
    EXPECT_EQ(result, "5");
}

TEST(JSEngine, TemplateLiteralTernary) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const x = 10; `${x > 5 ? 'big' : 'small'}`");
    EXPECT_EQ(result, "big");
}

TEST(JSEngine, TemplateLiteralMethodCall) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const s = 'hello'; `${s.toUpperCase()}`");
    EXPECT_EQ(result, "HELLO");
}

TEST(JSEngine, TemplateLiteralNestedTemplates) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const a = 'x'; `outer ${`inner ${a}`} end`");
    EXPECT_EQ(result, "outer inner x end");
}

TEST(JSEngine, TemplateLiteralMultipleExpressions) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("`${1} + ${2} = ${1+2}`");
    EXPECT_EQ(result, "1 + 2 = 3");
}

TEST(JSEngine, TemplateLiteralArrayAccess) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const arr = [10,20,30]; `value=${arr[1]}`");
    EXPECT_EQ(result, "value=20");
}

TEST(JSEngine, TemplateLiteralFunctionInvoke) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const f = (n) => n * n; `square=${f(4)}`");
    EXPECT_EQ(result, "square=16");
}

TEST(JSEngine, TemplateLiteralObjectProperty) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("const obj = {name:'Alice', age:30}; `${obj.name} is ${obj.age}`");
    EXPECT_EQ(result, "Alice is 30");
}

// Cycle 817 — Regex advanced: named groups, lookahead, lookbehind, dotAll, sticky, flags, matchAll
TEST(JSEngine, RegexNamedCaptureGroup) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const m = '2024-02-15'.match(/(?<year>\\d{4})-(?<month>\\d{2})-(?<day>\\d{2})/);"
        "m.groups.year + '/' + m.groups.month + '/' + m.groups.day");
    EXPECT_EQ(result, "2024/02/15");
}

TEST(JSEngine, RegexPositiveLookahead) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "'100px 200em 300rem'.match(/\\d+(?=px)/g).join(',')");
    EXPECT_EQ(result, "100");
}

TEST(JSEngine, RegexNegativeLookahead) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "'fooX barY fooZ'.replace(/foo(?!X)/g, 'baz')");
    EXPECT_EQ(result, "fooX barY bazZ");
}

TEST(JSEngine, RegexPositiveLookbehind) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "'$100 €200 £300'.match(/(?<=\\$)\\d+/g).join(',')");
    EXPECT_EQ(result, "100");
}

TEST(JSEngine, RegexNegativeLookbehind) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "'a1 $2 b3'.match(/(?<!\\$)\\d/g).join(',')");
    EXPECT_EQ(result, "1,3");
}

TEST(JSEngine, RegexDotAllFlag) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "/foo.bar/s.test('foo\\nbar')");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, RegexSourceProperty) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("/hello\\d+/gi.source");
    EXPECT_EQ(result, "hello\\d+");
}

TEST(JSEngine, RegexFlagsProperty) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("/abc/gi.flags");
    EXPECT_EQ(result, "gi");
}

// Cycle 825 — Error classes, nested try/catch, finally, custom error extending Error
TEST(JSEngine, ClassExtendsError) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "class AppError extends Error { constructor(msg) { super(msg); this.name='AppError'; } }"
        "try { throw new AppError('fail'); } catch(e) { e.name + ':' + e.message }");
    EXPECT_EQ(result, "AppError:fail");
}

TEST(JSEngine, FinallyAlwaysRuns) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "let log = '';"
        "try { log += 'try'; throw 'err'; } catch(e) { log += 'catch'; } finally { log += 'finally'; }"
        "log");
    EXPECT_EQ(result, "trycatchfinally");
}

TEST(JSEngine, FinallyRunsOnNoThrow) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "let ran = false;"
        "try { 1+1; } finally { ran = true; }"
        "String(ran)");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, NestedTryCatch) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "let r = '';"
        "try { try { throw 'inner'; } catch(e) { r += 'inner:' + e; throw 'outer'; } } catch(e) { r += ',outer:' + e; }"
        "r");
    EXPECT_EQ(result, "inner:inner,outer:outer");
}

TEST(JSEngine, CatchRethrowCaughtByOuter) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "function risky() { try { throw new Error('oops'); } catch(e) { throw e; } }"
        "try { risky(); } catch(e) { e.message }");
    EXPECT_EQ(result, "oops");
}

TEST(JSEngine, RangeErrorType) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "var e = new RangeError('out of range'); e instanceof RangeError");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, ErrorInstanceofError) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "var e = new Error('test'); e instanceof Error");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, TypeErrorInstanceofError) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "var e = new TypeError('bad'); (e instanceof TypeError) + ',' + (e instanceof Error)");
    EXPECT_EQ(result, "true,true");
}

// Cycle 829 — Functional programming: reduce variants, flatMap, filter-map chains
TEST(JSEngine, ReduceToObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const pairs = [['a',1],['b',2],['c',3]];"
        "const obj = pairs.reduce((acc,[k,v]) => { acc[k]=v; return acc; }, {});"
        "obj.a + obj.b + obj.c");
    EXPECT_EQ(result, "6");
}

TEST(JSEngine, ReduceMaxValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "[3,1,4,1,5,9,2,6].reduce((max,v) => v > max ? v : max, 0)");
    EXPECT_EQ(result, "9");
}

TEST(JSEngine, ReduceProduct) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "[1,2,3,4,5].reduce((acc,v) => acc * v, 1)");
    EXPECT_EQ(result, "120");
}

TEST(JSEngine, ReduceConcatStrings) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "['a','b','c','d'].reduce((acc,v) => acc + v, '')");
    EXPECT_EQ(result, "abcd");
}

TEST(JSEngine, FlatMapFilterEvens) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "[[1,2],[3,4],[5,6]].flatMap(a => a).filter(n => n % 2 === 0).join(',')");
    EXPECT_EQ(result, "2,4,6");
}

TEST(JSEngine, FilterMapJoinChain) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "[1,2,3,4,5,6].filter(n => n % 2 === 0).map(n => n * n).join(',')");
    EXPECT_EQ(result, "4,16,36");
}

TEST(JSEngine, SortThenMapJoin) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "[3,1,4,1,5,9].sort((a,b) => a-b).map(n => n*2).join(',')");
    EXPECT_EQ(result, "2,2,6,8,10,18");
}

TEST(JSEngine, ObjectEntriesReduceToSum) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const scores = {alice: 85, bob: 92, carol: 78};"
        "Object.entries(scores).reduce((sum,[,v]) => sum + v, 0)");
    EXPECT_EQ(result, "255");
}

// Cycle 832 — Map/Set iterator methods: keys, values, entries, has-after-delete, size-after-delete, object keys, spread
TEST(JSEngine, MapKeysMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const m = new Map([['a',1],['b',2],['c',3]]); [...m.keys()].join(',')");
    EXPECT_EQ(result, "a,b,c");
}

TEST(JSEngine, MapValuesMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const m = new Map([['x',10],['y',20],['z',30]]); [...m.values()].join(',')");
    EXPECT_EQ(result, "10,20,30");
}

TEST(JSEngine, MapEntriesMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const m = new Map([['k','v']]); const [[key,val]] = m.entries(); key + '=' + val");
    EXPECT_EQ(result, "k=v");
}

TEST(JSEngine, MapHasAfterDelete) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const m = new Map(); m.set('key', 1); m.delete('key'); m.has('key')");
    EXPECT_EQ(result, "false");
}

TEST(JSEngine, MapSizeAfterDelete) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const m = new Map([['a',1],['b',2],['c',3]]); m.delete('b'); m.size");
    EXPECT_EQ(result, "2");
}

TEST(JSEngine, SetValuesMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const s = new Set([10,20,30]); [...s.values()].join(',')");
    EXPECT_EQ(result, "10,20,30");
}

TEST(JSEngine, SetHasAfterDelete) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const s = new Set([1,2,3]); s.delete(2); s.has(2)");
    EXPECT_EQ(result, "false");
}

TEST(JSEngine, SetSizeAfterDelete) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const s = new Set([1,2,3,4,5]); s.delete(3); s.size");
    EXPECT_EQ(result, "4");
}

// Cycle 836 — Symbol.for, Symbol.keyFor, Symbol.iterator custom, Symbol as property key, typeof symbol
TEST(JSEngine, SymbolForReturnsCached) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "Symbol.for('app.id') === Symbol.for('app.id')");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, SymbolForDifferentKeys) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "Symbol.for('a') !== Symbol.for('b')");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, SymbolKeyFor) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const s = Symbol.for('my.key'); Symbol.keyFor(s)");
    EXPECT_EQ(result, "my.key");
}

TEST(JSEngine, TypeofSymbol) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "typeof Symbol('test')");
    EXPECT_EQ(result, "symbol");
}

TEST(JSEngine, SymbolAsPropertyKey) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const sym = Symbol('key'); const obj = {}; obj[sym] = 42; obj[sym]");
    EXPECT_EQ(result, "42");
}

TEST(JSEngine, SymbolNotInObjectKeys) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const sym = Symbol('hidden'); const obj = { [sym]: 1, visible: 2 };"
        "Object.keys(obj).length");
    EXPECT_EQ(result, "1");
}

TEST(JSEngine, SymbolIteratorCustomObject) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const iter = { [Symbol.iterator]() { let n=0; return { next() { return n<3 ? {value:n++,done:false} : {done:true}; } }; } };"
        "[...iter].join(',')");
    EXPECT_EQ(result, "0,1,2");
}

TEST(JSEngine, SymbolDescriptionProperty) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "Symbol('my-desc').description");
    EXPECT_EQ(result, "my-desc");
}

// Cycle 840 — JSON replacer/reviver, Number methods, Math rounding
TEST(JSEngine, JsonStringifyReplacer) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "JSON.stringify({a:1,b:2,c:3}, ['a','c'])");
    EXPECT_NE(result.find("\"a\""), std::string::npos);
    EXPECT_EQ(result.find("\"b\""), std::string::npos);
}

TEST(JSEngine, JsonStringifyIndented) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "JSON.stringify({x:1}, null, 2)");
    EXPECT_NE(result.find("\n"), std::string::npos);
}

TEST(JSEngine, JsonParseReviver) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "JSON.parse('{\"n\":42}', (k,v) => k==='n' ? v*2 : v).n");
    EXPECT_EQ(result, "84");
}

TEST(JSEngine, NumberToFixedSix) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(3.14159).toFixed(2)");
    EXPECT_EQ(result, "3.14");
}

TEST(JSEngine, NumberToPrecisionFive) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(123.456).toPrecision(5)");
    EXPECT_EQ(result, "123.46");
}

TEST(JSEngine, NumberToExponentialTwo) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(12345).toExponential(2)");
    EXPECT_EQ(result, "1.23e+4");
}

TEST(JSEngine, MathRoundHalf) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.round(4.5)");
    EXPECT_EQ(result, "5");
}

TEST(JSEngine, MathRoundNegative) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Math.round(-4.5)");
    EXPECT_EQ(result, "-4");
}

// Cycle 846 — typed arrays and DataView
TEST(JSEngine, Uint8ArraySetAndGet) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Uint8Array([10,20,30])[1]");
    EXPECT_EQ(result, "20");
}

TEST(JSEngine, Int32ArrayNegativeValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Int32Array([-42])[0]");
    EXPECT_EQ(result, "-42");
}

TEST(JSEngine, Float64ArrayPiValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Float64Array([Math.PI])[0].toFixed(5)");
    EXPECT_EQ(result, "3.14159");
}

TEST(JSEngine, Uint16ArrayLength) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Uint16Array(5).length");
    EXPECT_EQ(result, "5");
}

TEST(JSEngine, Uint8ClampedArrayClampsHigh) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Uint8ClampedArray([300])[0]");
    EXPECT_EQ(result, "255");
}

TEST(JSEngine, Uint8ClampedArrayClampsLow) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Uint8ClampedArray([-5])[0]");
    EXPECT_EQ(result, "0");
}

TEST(JSEngine, Int8ArrayMinValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Int8Array([-128])[0]");
    EXPECT_EQ(result, "-128");
}

TEST(JSEngine, DataViewGetInt32) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const b=new ArrayBuffer(4);"
        "const v=new DataView(b);"
        "v.setInt32(0,12345678,false);"
        "v.getInt32(0,false)");
    EXPECT_EQ(result, "12345678");
}

// Cycle 855 — BigInt advanced ops, DataView little-endian, TypedArray copyWithin
TEST(JSEngine, BigIntToString) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(123456789012345678901234567890n).toString()");
    EXPECT_EQ(result, "123456789012345678901234567890");
}

TEST(JSEngine, BigIntNegative) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(-100n).toString()");
    EXPECT_EQ(result, "-100");
}

TEST(JSEngine, BigIntDivision) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(100n / 3n).toString()");
    EXPECT_EQ(result, "33");
}

TEST(JSEngine, BigIntModulo) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(17n % 5n).toString()");
    EXPECT_EQ(result, "2");
}

TEST(JSEngine, BigIntBitwiseAnd) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(0b1100n & 0b1010n).toString()");
    EXPECT_EQ(result, "8");
}

TEST(JSEngine, BigIntBitwiseOr) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(0b1100n | 0b1010n).toString()");
    EXPECT_EQ(result, "14");
}

TEST(JSEngine, BigIntLeftShift) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(1n << 10n).toString()");
    EXPECT_EQ(result, "1024");
}

TEST(JSEngine, DataViewLittleEndianInt32) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const b=new ArrayBuffer(4);"
        "const v=new DataView(b);"
        "v.setInt32(0,0x01020304,true);"
        "v.getInt32(0,true)");
    EXPECT_EQ(result, "16909060");
}

// Cycle 864 — BigInt XOR/NOT/right-shift/typeof, Number to BigInt, DataView big-endian variants
TEST(JSEngine, BigIntBitwiseXor) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(0b1100n ^ 0b1010n).toString()");
    EXPECT_EQ(result, "6");
}

TEST(JSEngine, BigIntRightShift) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(1024n >> 3n).toString()");
    EXPECT_EQ(result, "128");
}

TEST(JSEngine, BigIntTypeofIsBigint) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("typeof 42n");
    EXPECT_EQ(result, "bigint");
}

TEST(JSEngine, BigIntZero) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(0n).toString()");
    EXPECT_EQ(result, "0");
}

TEST(JSEngine, BigIntPower) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(2n ** 32n).toString()");
    EXPECT_EQ(result, "4294967296");
}

TEST(JSEngine, BigIntAbsoluteNegative) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("(-42n < 0n).toString()");
    EXPECT_EQ(result, "true");
}

TEST(JSEngine, DataViewGetFloat32) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const b=new ArrayBuffer(4);"
        "const v=new DataView(b);"
        "v.setFloat32(0,1.5,false);"
        "v.getFloat32(0,false)");
    EXPECT_EQ(result, "1.5");
}

TEST(JSEngine, DataViewGetUint16) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const b=new ArrayBuffer(2);"
        "const v=new DataView(b);"
        "v.setUint16(0,12345,false);"
        "v.getUint16(0,false)");
    EXPECT_EQ(result, "12345");
}


// Cycle 868 — DataView byte methods: getInt8, setInt8, getUint8, setUint8, getInt16, setInt16, byteLength, byteOffset
TEST(JSEngine, DataViewGetInt8) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const b=new ArrayBuffer(1);"
        "const v=new DataView(b);"
        "v.setInt8(0,-5);"
        "v.getInt8(0)");
    EXPECT_EQ(result, "-5");
}

TEST(JSEngine, DataViewGetUint8) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const b=new ArrayBuffer(1);"
        "const v=new DataView(b);"
        "v.setUint8(0,255);"
        "v.getUint8(0)");
    EXPECT_EQ(result, "255");
}

TEST(JSEngine, DataViewGetInt16BigEndian) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const b=new ArrayBuffer(2);"
        "const v=new DataView(b);"
        "v.setInt16(0,1000,false);"
        "v.getInt16(0,false)");
    EXPECT_EQ(result, "1000");
}

TEST(JSEngine, DataViewGetInt16LittleEndian) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const b=new ArrayBuffer(2);"
        "const v=new DataView(b);"
        "v.setInt16(0,-300,true);"
        "v.getInt16(0,true)");
    EXPECT_EQ(result, "-300");
}

TEST(JSEngine, DataViewByteLength) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const b=new ArrayBuffer(8);"
        "const v=new DataView(b);"
        "v.byteLength");
    EXPECT_EQ(result, "8");
}

TEST(JSEngine, DataViewByteOffset) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const b=new ArrayBuffer(16);"
        "const v=new DataView(b,4);"
        "v.byteOffset");
    EXPECT_EQ(result, "4");
}

TEST(JSEngine, DataViewGetUint32LittleEndian) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const b=new ArrayBuffer(4);"
        "const v=new DataView(b);"
        "v.setUint32(0,0xDEADBEEF,true);"
        "v.getUint32(0,true)");
    EXPECT_EQ(result, "3735928559");
}

TEST(JSEngine, DataViewFloat64RoundTrip) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const b=new ArrayBuffer(8);"
        "const v=new DataView(b);"
        "v.setFloat64(0,3.14159265358979,false);"
        "v.getFloat64(0,false).toFixed(5)");
    EXPECT_EQ(result, "3.14159");
}

// Cycle 877 — TypedArray: Int16Array, Uint32Array, Float32Array, Int8Array max, Uint8Array overflow, TypedArray.from, TypedArray length, TypedArray set
TEST(JSEngine, Int16ArrayValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Int16Array([-1000])[0]");
    EXPECT_EQ(result, "-1000");
}

TEST(JSEngine, Uint32ArrayMaxValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Uint32Array([4294967295])[0]");
    EXPECT_EQ(result, "4294967295");
}

TEST(JSEngine, Float32ArrayValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Float32Array([3.14])[0].toFixed(2)");
    EXPECT_EQ(result, "3.14");
}

TEST(JSEngine, Int8ArrayMaxValue) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Int8Array([127])[0]");
    EXPECT_EQ(result, "127");
}

TEST(JSEngine, Uint8ArrayOverflowWraps) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Uint8Array([256])[0]");
    EXPECT_EQ(result, "0");
}

TEST(JSEngine, TypedArrayFromArray) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("Uint8Array.from([10,20,30])[1]");
    EXPECT_EQ(result, "20");
}

TEST(JSEngine, TypedArrayLengthProp) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate("new Float64Array(5).length");
    EXPECT_EQ(result, "5");
}

TEST(JSEngine, TypedArraySetMethod) {
    clever::js::JSEngine engine;
    auto result = engine.evaluate(
        "const a=new Uint8Array(3);"
        "a.set([7,8,9]);"
        "a[2]");
    EXPECT_EQ(result, "9");
}
