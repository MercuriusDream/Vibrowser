#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "browser/render/canvas.h"

namespace browser::layout {
struct LayoutBox;
}  // namespace browser::layout

namespace browser::render {

enum class RenderMode {
    Headless,  // PPM image output
    Shell,     // Text-based terminal output
};

struct RenderMetadata {
    int width = 0;
    int height = 0;
    std::size_t pixel_count = 0;
    std::size_t byte_count = 0;
    double render_duration_ms = 0.0;
};

Canvas render_to_canvas(const browser::layout::LayoutBox& root, int width, int height);
Canvas render_to_canvas(const browser::layout::LayoutBox& root, int width, int height,
                        RenderMetadata& metadata);

bool write_ppm(const Canvas& canvas, const std::string& path);

bool write_render_metadata(const RenderMetadata& metadata, const std::string& path);

std::string render_to_text(const browser::layout::LayoutBox& root, int width);

enum class RenderStage {
    CanvasInit,
    BackgroundResolve,
    Paint,
    Complete,
};

const char* render_stage_name(RenderStage stage);

struct RenderTraceEntry {
    RenderStage stage;
    std::chrono::steady_clock::time_point entered_at;
    double elapsed_since_prev_ms = 0.0;
};

struct RenderTrace {
    std::vector<RenderTraceEntry> entries;

    void record(RenderStage stage);

    bool is_reproducible_with(const RenderTrace& other,
                              double tolerance_factor = 3.0) const;
};

Canvas render_to_canvas_traced(const browser::layout::LayoutBox& root, int width, int height,
                               RenderTrace& trace);
Canvas render_to_canvas_traced(const browser::layout::LayoutBox& root, int width, int height,
                               RenderMetadata& metadata, RenderTrace& trace);

bool write_render_trace(const RenderTrace& trace, const std::string& path);

}  // namespace browser::render

