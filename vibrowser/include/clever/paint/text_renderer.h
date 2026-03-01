#pragma once
#include <clever/paint/display_list.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations for CoreText/CoreGraphics types used by render_single_line
typedef const struct __CTFont* CTFontRef;
typedef struct CGColor* CGColorRef;
typedef struct CGColorSpace* CGColorSpaceRef;
typedef const struct __CTFontDescriptor* CTFontDescriptorRef;

namespace clever::paint {

// Renders text glyphs into an RGBA pixel buffer using macOS CoreText.
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    // Register a web font (downloaded @font-face data) with CoreText.
    // family_name: the CSS font-family name (e.g., "Open Sans")
    // font_data: raw font file bytes (TTF, OTF, or WOFF)
    // weight: CSS font-weight (100-900), 0 means unspecified
    // italic: whether this is an italic variant
    // Returns true if the font was successfully registered.
    static bool register_font(const std::string& family_name,
                              const std::vector<uint8_t>& font_data,
                              int weight = 0, bool italic = false);

    // Check if a web font is registered for the given family name.
    static bool has_registered_font(const std::string& family_name);

    // Clear all registered web fonts (useful for testing).
    static void clear_registered_fonts();

    // Render text string into a pixel buffer at position (x, y).
    // Handles newlines by rendering each line separately.
    void render_text(const std::string& text, float x, float y,
                     float font_size, const Color& color,
                     uint8_t* buffer, int buffer_width, int buffer_height,
                     const std::string& font_family = "",
                     int font_weight = 400, bool font_italic = false,
                     float letter_spacing = 0,
                     const std::string& font_feature_settings = "",
                     const std::string& font_variation_settings = "",
                     int text_rendering = 0, int font_kerning = 0,
                     int font_optical_sizing = 0, float word_spacing = 0,
                     float clip_x = -1, float clip_y = -1,
                     float clip_w = -1, float clip_h = -1);

    // Measure text width for a given string and font size.
    float measure_text_width(const std::string& text, float font_size,
                             const std::string& font_family = "");

    // Enhanced overload: measure text width with full font properties and letter spacing.
    float measure_text_width(const std::string& text, float font_size,
                             const std::string& font_family,
                             int font_weight, bool font_italic,
                             float letter_spacing, float word_spacing = 0);

private:
    // Render a single line (no newlines) at the given position
    void render_single_line(const std::string& text, float x, float y,
                            float font_size, const Color& color,
                            uint8_t* buffer, int buffer_width, int buffer_height,
                            CTFontRef font, CGColorRef cg_color, CGColorSpaceRef colorspace,
                            float letter_spacing = 0, int text_rendering = 0,
                            int font_kerning = 0, float word_spacing = 0,
                            float clip_x = -1, float clip_y = -1,
                            float clip_w = -1, float clip_h = -1);
};

} // namespace clever::paint
