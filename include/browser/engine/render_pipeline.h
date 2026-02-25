#pragma once

#include "browser/css/css_parser.h"
#include "browser/html/dom.h"
#include "browser/layout/layout_engine.h"
#include "browser/render/canvas.h"
#include "browser/render/renderer.h"

#include <memory>
#include <string>

namespace browser::engine {

struct RerenderResult {
    bool ok = false;
    std::string message;
    int render_count = 0;
};

class RenderPipeline {
public:
    RenderPipeline(std::unique_ptr<browser::html::Node> document,
                   browser::css::Stylesheet stylesheet,
                   int viewport_width, int viewport_height);

    browser::html::Node& document();
    const browser::html::Node& document() const;

    const browser::layout::LayoutBox& layout() const;
    const browser::render::Canvas& canvas() const;

    RerenderResult rerender();

    int render_count() const;

private:
    std::unique_ptr<browser::html::Node> document_;
    browser::css::Stylesheet stylesheet_;
    int viewport_width_;
    int viewport_height_;
    browser::layout::LayoutBox layout_;
    browser::render::Canvas canvas_;
    int render_count_ = 0;
};

}  // namespace browser::engine
