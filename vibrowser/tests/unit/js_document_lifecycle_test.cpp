#include <clever/js/js_dom_bindings.h>
#include <clever/js/js_engine.h>
#include <clever/html/tree_builder.h>
#include <clever/paint/image_fetch.h>
#include <gtest/gtest.h>

#include <memory>

namespace clever::js {
void dispatch_visibility_change(JSContext* ctx);
}

namespace clever::paint {

float canvas_render_text(const std::string&, float, float, float, const std::string&, int,
                         bool, uint32_t, float, int, int, uint8_t*, int, int, float) {
    return 0.0f;
}

float canvas_measure_text(const std::string&, float, const std::string&, int, bool) {
    return 0.0f;
}

float canvas_render_text_stroke(const std::string&, float, float, float, const std::string&,
                                int, bool, uint32_t, float, int, int, float, uint8_t*, int,
                                int, float) {
    return 0.0f;
}

JSImageData fetch_image_for_js(const std::string&) {
    return {};
}

}  // namespace clever::paint

namespace {

class ScopedDocumentBindings {
public:
    explicit ScopedDocumentBindings(const std::string& html)
        : document_(clever::html::parse(html)) {
        EXPECT_NE(document_, nullptr);
        clever::js::install_dom_bindings(engine_.context(), document_.get());
    }

    ~ScopedDocumentBindings() {
        clever::js::cleanup_dom_bindings(engine_.context());
    }

    clever::js::JSEngine& engine() { return engine_; }

private:
    clever::js::JSEngine engine_;
    std::unique_ptr<clever::html::SimpleNode> document_;
};

TEST(JSDocumentLifecycle, VisibleDocumentDoesNotFireInitialVisibilityChangeV2068) {
    ScopedDocumentBindings bindings("<html><body><p>hello</p></body></html>");

    const auto result = bindings.engine().evaluate(R"(
        (() => {
            let fired = 0;
            window.addEventListener('visibilitychange', () => { fired += 1; });
            return JSON.stringify({
                state: document.visibilityState,
                hidden: document.hidden,
                fired,
            });
        })()
    )");

    EXPECT_FALSE(bindings.engine().has_error()) << bindings.engine().last_error();
    EXPECT_EQ(result, R"({"state":"visible","hidden":false,"fired":0})");
}

TEST(JSDocumentLifecycle, VisibilityChangeRequiresStateFlipV2068) {
    ScopedDocumentBindings bindings("<html><body><p>hello</p></body></html>");

    const auto initial_result = bindings.engine().evaluate(R"(
        (() => {
            window.__visibilityEvents = [];
            window.addEventListener('visibilitychange', () => {
                window.__visibilityEvents.push(`${document.visibilityState}:${document.hidden}`);
            });
            return window.__visibilityEvents.length;
        })()
    )");
    ASSERT_FALSE(bindings.engine().has_error()) << bindings.engine().last_error();
    EXPECT_EQ(initial_result, "0");

    clever::js::dispatch_visibility_change(bindings.engine().context());
    auto result = bindings.engine().evaluate("window.__visibilityEvents.length");
    ASSERT_FALSE(bindings.engine().has_error()) << bindings.engine().last_error();
    EXPECT_EQ(result, "0");

    bindings.engine().evaluate("document.visibilityState = 'hidden'; document.hidden = true;");
    ASSERT_FALSE(bindings.engine().has_error()) << bindings.engine().last_error();
    clever::js::dispatch_visibility_change(bindings.engine().context());

    result = bindings.engine().evaluate("JSON.stringify(window.__visibilityEvents)");
    ASSERT_FALSE(bindings.engine().has_error()) << bindings.engine().last_error();
    EXPECT_EQ(result, R"(["hidden:true"])");

    clever::js::dispatch_visibility_change(bindings.engine().context());
    result = bindings.engine().evaluate("window.__visibilityEvents.length");
    ASSERT_FALSE(bindings.engine().has_error()) << bindings.engine().last_error();
    EXPECT_EQ(result, "1");

    bindings.engine().evaluate("document.visibilityState = 'visible'; document.hidden = false;");
    ASSERT_FALSE(bindings.engine().has_error()) << bindings.engine().last_error();
    clever::js::dispatch_visibility_change(bindings.engine().context());

    result = bindings.engine().evaluate("JSON.stringify(window.__visibilityEvents)");
    ASSERT_FALSE(bindings.engine().has_error()) << bindings.engine().last_error();
    EXPECT_EQ(result, R"(["hidden:true","visible:false"])");
}

}  // namespace
