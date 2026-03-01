// canvas_text_bridge.mm
// CoreText-based implementation of the Canvas 2D text rendering bridge.
// This file is compiled as Objective-C++ so it can freely use
// CoreFoundation/CoreGraphics/CoreText APIs.

#include <clever/paint/canvas_text_bridge.h>
#include <clever/paint/text_renderer.h>   // for create_web_font / has_registered_font helpers

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>

namespace clever::paint {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Map a CSS font-family string to a CoreText font name.
// Generic families are mapped to known macOS equivalents.
static CFStringRef cf_font_name_for_family(const std::string& family) {
    // Exact-match common generic families
    if (family == "monospace" || family == "Courier" ||
        family == "Courier New" || family == "Menlo" || family == "Monaco") {
        return CFSTR("Menlo");
    }
    if (family == "serif" || family == "Times" || family == "Times New Roman" ||
        family == "Georgia") {
        return CFSTR("Times New Roman");
    }
    // Default: sans-serif / Helvetica / Arial
    if (family == "Arial" || family == "Helvetica Neue") {
        return CFSTR("Helvetica Neue");
    }
    return CFSTR("Helvetica");
}

// Parse a CSS font string like "bold 16px Arial" into components.
static void parse_canvas_font(const std::string& font_str,
                               float& out_size,
                               std::string& out_family,
                               int& out_weight,
                               bool& out_italic) {
    out_size   = 10.0f;
    out_family = "sans-serif";
    out_weight = 400;
    out_italic = false;

    // Tokenise the font string
    std::string tok;
    std::istringstream iss(font_str);
    std::vector<std::string> tokens;
    while (iss >> tok) tokens.push_back(tok);

    // Walk through tokens:
    //   keywords before the size token set style/weight
    //   the first token ending with "px"/"em"/"pt" is the size
    //   everything after is treated as the family
    bool found_size = false;
    size_t family_start = 0;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];

        if (!found_size) {
            // Style / variant / weight keywords
            if (t == "italic" || t == "oblique") { out_italic = true; continue; }
            if (t == "bold")   { out_weight = 700; continue; }
            if (t == "bolder") { out_weight = 900; continue; }
            if (t == "lighter") { out_weight = 300; continue; }
            if (t == "normal") continue;

            // Numeric weight
            bool all_digits = !t.empty();
            for (char c : t) if (c < '0' || c > '9') { all_digits = false; break; }
            if (all_digits && t.size() >= 3) {
                try { out_weight = std::stoi(t); } catch (...) {}
                continue;
            }

            // Size token: ends with "px", "pt", "em", or contains "/" (line-height)
            auto px_pos = t.find("px");
            auto pt_pos = t.find("pt");
            auto em_pos = t.find("em");
            auto sl_pos = t.find('/');  // "16px/1.5"

            std::string size_part = t;
            if (sl_pos != std::string::npos) size_part = t.substr(0, sl_pos);

            float sz = 0;
            bool parsed = false;
            try {
                if (px_pos != std::string::npos) {
                    sz = std::stof(size_part.substr(0, px_pos));
                    parsed = true;
                } else if (pt_pos != std::string::npos) {
                    sz = std::stof(size_part.substr(0, pt_pos)) * (96.0f / 72.0f);
                    parsed = true;
                } else if (em_pos != std::string::npos) {
                    sz = std::stof(size_part.substr(0, em_pos)) * 16.0f; // assume 16px base
                    parsed = true;
                }
            } catch (...) {}

            if (parsed && sz > 0) {
                out_size = sz;
                found_size = true;
                family_start = i + 1;
            }
        }
    }

    // Remainder is the font family
    if (found_size && family_start < tokens.size()) {
        std::string fam;
        for (size_t i = family_start; i < tokens.size(); ++i) {
            if (i > family_start) fam += ' ';
            fam += tokens[i];
        }
        // Strip surrounding quotes if present
        if (fam.size() >= 2 &&
            ((fam.front() == '"' && fam.back() == '"') ||
             (fam.front() == '\'' && fam.back() == '\''))) {
            fam = fam.substr(1, fam.size() - 2);
        }
        if (!fam.empty()) out_family = fam;
    }
}

// Create a CTFont for the given parameters.
// Returns nullptr on failure; caller must CFRelease.
static CTFontRef create_ctfont(const std::string& family,
                                float font_size,
                                int font_weight,
                                bool font_italic) {
    if (font_size < 1.0f) font_size = 1.0f;

    // Note: web fonts registered via TextRenderer::register_font() are accessible
    // through CTFontCreateWithName because register_font() calls
    // CTFontManagerRegisterGraphicsFont, which makes them available system-wide.
    // We try the family name directly first; cf_font_name_for_family maps generics.

    // Try the family name directly first — this works for both registered web fonts
    // (added to the CT font registry via CTFontManagerRegisterGraphicsFont) and
    // explicitly named system fonts like "Arial", "Georgia", etc.
    CFStringRef direct_name = CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(family.data()),
        static_cast<CFIndex>(family.size()),
        kCFStringEncodingUTF8, false);
    CTFontRef font = nullptr;
    if (direct_name) {
        font = CTFontCreateWithName(direct_name, static_cast<CGFloat>(font_size), nullptr);
        CFRelease(direct_name);
    }
    // Fall back to mapped generic family name if direct lookup failed
    if (!font) {
        CFStringRef name = cf_font_name_for_family(family);
        font = CTFontCreateWithName(name, static_cast<CGFloat>(font_size), nullptr);
    }
    if (!font) {
        font = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem,
                                             static_cast<CGFloat>(font_size), nullptr);
    }
    if (!font) return nullptr;

    // Apply bold / italic traits
    CTFontSymbolicTraits traits = 0;
    if (font_weight >= 700) traits |= kCTFontBoldTrait;
    if (font_italic)        traits |= kCTFontItalicTrait;

    if (traits != 0) {
        CTFontRef styled = CTFontCreateCopyWithSymbolicTraits(
            font, 0.0, nullptr, traits, traits);
        if (styled) {
            CFRelease(font);
            font = styled;
        }
    }
    return font;
}

// Compute the actual pixel width of a text string using CoreText typographic bounds.
static float ct_measure_width(const std::string& text, CTFontRef font) {
    if (text.empty()) return 0.0f;

    CFStringRef cf_str = CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(text.data()),
        static_cast<CFIndex>(text.size()),
        kCFStringEncodingUTF8, false);
    if (!cf_str) return 0.0f;

    const void* keys[] = {kCTFontAttributeName};
    const void* vals[] = {font};
    CFDictionaryRef attrs = CFDictionaryCreate(
        kCFAllocatorDefault, keys, vals, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFAttributedStringRef attr_str =
        CFAttributedStringCreate(kCFAllocatorDefault, cf_str, attrs);
    CFRelease(attrs);
    CFRelease(cf_str);
    if (!attr_str) return 0.0f;

    CTLineRef line = CTLineCreateWithAttributedString(attr_str);
    CFRelease(attr_str);
    if (!line) return 0.0f;

    CGFloat ascent, descent, leading;
    double width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
    CFRelease(line);
    return static_cast<float>(width);
}

// ---------------------------------------------------------------------------
// canvas_render_text  — the main entry point
// ---------------------------------------------------------------------------
float canvas_render_text(const std::string& text,
                         float x, float y,
                         float font_size,
                         const std::string& font_family,
                         int font_weight,
                         bool font_italic,
                         uint32_t fill_color,  // ARGB
                         float global_alpha,
                         int text_align,
                         int text_baseline,
                         uint8_t* buffer,
                         int buf_width,
                         int buf_height,
                         float max_width) {
    if (text.empty() || !buffer || buf_width <= 0 || buf_height <= 0) return 0.0f;
    if (font_size < 1.0f) font_size = 1.0f;

    // ------------------------------------------------------------------
    // 1. Create the CTFont
    // ------------------------------------------------------------------
    CTFontRef font = create_ctfont(font_family, font_size, font_weight, font_italic);
    if (!font) return 0.0f;

    // ------------------------------------------------------------------
    // 2. Build color from ARGB fill_color + global_alpha
    // ------------------------------------------------------------------
    CGFloat cr = static_cast<CGFloat>((fill_color >> 16) & 0xFF) / 255.0;
    CGFloat cg = static_cast<CGFloat>((fill_color >> 8)  & 0xFF) / 255.0;
    CGFloat cb = static_cast<CGFloat>( fill_color        & 0xFF) / 255.0;
    CGFloat ca = static_cast<CGFloat>((fill_color >> 24) & 0xFF) / 255.0
                 * static_cast<CGFloat>(global_alpha);

    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGFloat components[4] = {cr, cg, cb, ca};
    CGColorRef cg_color = CGColorCreate(colorspace, components);

    // ------------------------------------------------------------------
    // 3. Measure text width for text-align adjustment
    // ------------------------------------------------------------------
    float text_width = ct_measure_width(text, font);

    // Apply max_width scaling via horizontal scale transform if needed
    float scale_x = 1.0f;
    if (max_width > 0.0f && text_width > max_width && text_width > 0.0f) {
        scale_x = max_width / text_width;
        text_width = max_width;
    }

    // ------------------------------------------------------------------
    // 4. Compute draw_x based on textAlign
    // ------------------------------------------------------------------
    float draw_x = x;
    switch (text_align) {
        case 1: draw_x = x - text_width * 0.5f; break;   // center
        case 2: // right
        case 3: // end
            draw_x = x - text_width; break;
        default: break; // start / left
    }

    // ------------------------------------------------------------------
    // 5. Compute draw_y (baseline in canvas top-left coords).
    //    CoreText uses bottom-left origin, so we convert below.
    // ------------------------------------------------------------------
    CGFloat ct_ascent  = CTFontGetAscent(font);
    CGFloat ct_descent = CTFontGetDescent(font);  // positive value
    float em_height = static_cast<float>(ct_ascent + ct_descent);

    float baseline_y;  // canvas-space baseline (distance from top edge)
    switch (text_baseline) {
        case 1:  // top — y is top of em-box
            baseline_y = y + static_cast<float>(ct_ascent);
            break;
        case 2:  // hanging — approximately same as top
            baseline_y = y + static_cast<float>(ct_ascent);
            break;
        case 3:  // middle — y is center of em-box
            baseline_y = y + em_height * 0.5f - static_cast<float>(ct_descent) + static_cast<float>(ct_ascent * 0.5f);
            // Simpler: y is mid-point between ascent and -descent
            baseline_y = y + static_cast<float>(ct_ascent - (ct_ascent + ct_descent) * 0.5f);
            break;
        case 4:  // ideographic — treat as bottom of em-box
        case 5:  // bottom — y is bottom of em-box (descent line)
            baseline_y = y - static_cast<float>(ct_descent);
            break;
        default: // 0 = alphabetic — y IS the baseline
            baseline_y = y;
            break;
    }

    // ------------------------------------------------------------------
    // 6. Build the attributed string with font and color
    // ------------------------------------------------------------------
    CFStringRef cf_str = CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(text.data()),
        static_cast<CFIndex>(text.size()),
        kCFStringEncodingUTF8, false);

    if (!cf_str) {
        CGColorRelease(cg_color);
        CGColorSpaceRelease(colorspace);
        CFRelease(font);
        return text_width;
    }

    const void* attr_keys[] = {kCTFontAttributeName, kCTForegroundColorAttributeName};
    const void* attr_vals[] = {font, cg_color};
    CFDictionaryRef attrs = CFDictionaryCreate(
        kCFAllocatorDefault, attr_keys, attr_vals, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFAttributedStringRef attr_str =
        CFAttributedStringCreate(kCFAllocatorDefault, cf_str, attrs);
    CFRelease(attrs);
    CFRelease(cf_str);

    if (!attr_str) {
        CGColorRelease(cg_color);
        CGColorSpaceRelease(colorspace);
        CFRelease(font);
        return text_width;
    }

    CTLineRef line = CTLineCreateWithAttributedString(attr_str);
    CFRelease(attr_str);

    if (!line) {
        CGColorRelease(cg_color);
        CGColorSpaceRelease(colorspace);
        CFRelease(font);
        return text_width;
    }

    // ------------------------------------------------------------------
    // 7. Create a CGBitmapContext wrapping the canvas pixel buffer.
    //    The buffer is RGBA (premultiplied last).
    //    CG uses bottom-left origin; canvas uses top-left.
    //    We convert baseline_y (top-left) to CG's bottom-left system:
    //      cg_baseline_y = buf_height - baseline_y
    // ------------------------------------------------------------------
    CGContextRef cg_ctx = CGBitmapContextCreate(
        buffer,
        static_cast<size_t>(buf_width),
        static_cast<size_t>(buf_height),
        8,                                // bits per component
        static_cast<size_t>(buf_width) * 4,  // bytes per row
        colorspace,
        kCGImageAlphaPremultipliedLast);

    if (!cg_ctx) {
        CFRelease(line);
        CGColorRelease(cg_color);
        CGColorSpaceRelease(colorspace);
        CFRelease(font);
        return text_width;
    }

    // Ensure identity text matrix (avoids stale state on some macOS versions)
    CGContextSetTextMatrix(cg_ctx, CGAffineTransformIdentity);
    CGContextSetTextDrawingMode(cg_ctx, kCGTextFill);
    CGContextSetShouldAntialias(cg_ctx, true);
    CGContextSetAllowsAntialiasing(cg_ctx, true);
    CGContextSetShouldSmoothFonts(cg_ctx, true);
    CGContextSetAllowsFontSmoothing(cg_ctx, true);

    // Convert from canvas (top-left) baseline to CG (bottom-left) baseline:
    //   cg_y = buf_height - baseline_y
    CGFloat cg_baseline_x = static_cast<CGFloat>(draw_x);
    CGFloat cg_baseline_y = static_cast<CGFloat>(buf_height) - static_cast<CGFloat>(baseline_y);

    // Apply horizontal scale if max_width clamping is needed
    if (scale_x != 1.0f) {
        CGContextSaveGState(cg_ctx);
        // Scale around the text origin
        CGContextTranslateCTM(cg_ctx, cg_baseline_x, cg_baseline_y);
        CGContextScaleCTM(cg_ctx, static_cast<CGFloat>(scale_x), 1.0);
        CGContextSetTextPosition(cg_ctx, 0, 0);
        CTLineDraw(line, cg_ctx);
        CGContextRestoreGState(cg_ctx);
    } else {
        CGContextSetTextPosition(cg_ctx, cg_baseline_x, cg_baseline_y);
        CTLineDraw(line, cg_ctx);
    }

    CGContextRelease(cg_ctx);
    CFRelease(line);
    CGColorRelease(cg_color);
    CGColorSpaceRelease(colorspace);
    CFRelease(font);

    return text_width;
}

// ---------------------------------------------------------------------------
// canvas_measure_text
// ---------------------------------------------------------------------------
float canvas_measure_text(const std::string& text,
                          float font_size,
                          const std::string& font_family,
                          int font_weight,
                          bool font_italic) {
    if (text.empty()) return 0.0f;
    CTFontRef font = create_ctfont(font_family, font_size, font_weight, font_italic);
    if (!font) return static_cast<float>(text.size()) * font_size * 0.6f;
    float w = ct_measure_width(text, font);
    CFRelease(font);
    return w;
}

} // namespace clever::paint
