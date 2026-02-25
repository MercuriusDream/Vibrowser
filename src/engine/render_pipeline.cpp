#include "browser/engine/render_pipeline.h"

namespace browser::engine {

RenderPipeline::RenderPipeline(std::unique_ptr<browser::html::Node> document,
                               browser::css::Stylesheet stylesheet,
                               int viewport_width, int viewport_height)
    : document_(std::move(document)),
      stylesheet_(std::move(stylesheet)),
      viewport_width_(viewport_width),
      viewport_height_(viewport_height) {
    rerender();
}

browser::html::Node& RenderPipeline::document() {
    return *document_;
}

const browser::html::Node& RenderPipeline::document() const {
    return *document_;
}

const browser::layout::LayoutBox& RenderPipeline::layout() const {
    return layout_;
}

const browser::render::Canvas& RenderPipeline::canvas() const {
    return canvas_;
}

RerenderResult RenderPipeline::rerender() {
    if (!document_) {
        return {false, "No document", render_count_};
    }

    layout_ = browser::layout::layout_document(*document_, stylesheet_, viewport_width_);
    canvas_ = browser::render::render_to_canvas(layout_, viewport_width_, viewport_height_);
    ++render_count_;

    return {true, "OK", render_count_};
}

int RenderPipeline::render_count() const {
    return render_count_;
}

}  // namespace browser::engine
