#pragma once
// canvas_text_bridge.h
// Plain C++ bridge between the Canvas 2D JS API and CoreText (macOS).
// Implemented in src/paint/canvas_text_bridge.mm so that CoreText/CoreGraphics
// headers are only seen by the Objective-C++ translation unit.
//
// All parameters use basic C/C++ types so this header can be safely included
// from ordinary .cpp files such as js_dom_bindings.cpp.

#include <cstdint>
#include <string>
#include <vector>

namespace clever::paint {

// Render a text string into an RGBA pixel buffer using CoreText.
//
// Parameters
// ----------
// text         : UTF-8 text to render
// x, y         : Canvas-space position. Meaning depends on text_baseline:
//                  alphabetic (0) — y is the baseline
//                  top (1)        — y is the top of the em-box
//                  middle (3)     — y is the middle of the em-box
//                  bottom (5)     — y is the bottom of the em-box
// font_size    : Point size of the font
// font_family  : CSS font-family string (e.g. "sans-serif", "Arial")
// font_weight  : CSS font-weight integer (100–900; 400 = regular, 700 = bold)
// font_italic  : Whether to use italic style
// fill_color   : ARGB color packed as uint32_t (same layout as Canvas2DState)
// global_alpha : Canvas global alpha multiplier (0.0–1.0)
// text_align   : 0 = start/left, 1 = center, 2/3 = right/end
// text_baseline: 0 = alphabetic, 1 = top, 2 = hanging, 3 = middle,
//                4 = ideographic, 5 = bottom
// buffer       : Pointer to the pixel buffer (RGBA, 4 bytes/pixel, row-major)
// buf_width    : Width of the buffer in pixels
// buf_height   : Height of the buffer in pixels
// max_width    : If > 0, scale text to fit within this width (CSS spec)
//
// Returns the actual rendered text width in pixels (useful for callers).
float canvas_render_text(const std::string& text,
                         float x, float y,
                         float font_size,
                         const std::string& font_family,
                         int font_weight,
                         bool font_italic,
                         uint32_t fill_color,
                         float global_alpha,
                         int text_align,
                         int text_baseline,
                         uint8_t* buffer,
                         int buf_width,
                         int buf_height,
                         float max_width = 0.0f);

// Measure the width (in pixels) of a text string using CoreText.
float canvas_measure_text(const std::string& text,
                          float font_size,
                          const std::string& font_family,
                          int font_weight,
                          bool font_italic);

} // namespace clever::paint
