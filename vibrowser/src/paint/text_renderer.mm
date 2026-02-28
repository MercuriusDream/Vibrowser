#include <clever/paint/text_renderer.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>

namespace clever::paint {

// ---- Web font registry ----
// Maps CSS font-family name to a CoreText font descriptor created from downloaded font data.
// Key format: "family_name" or "family_name:weight:italic" for weight/style-specific variants.
struct RegisteredWebFont {
    CTFontDescriptorRef descriptor = nullptr;
    int weight = 0;
    bool italic = false;
};

static std::unordered_map<std::string, std::vector<RegisteredWebFont>>& registered_fonts() {
    static std::unordered_map<std::string, std::vector<RegisteredWebFont>> fonts;
    return fonts;
}

static std::mutex& font_registry_mutex() {
    static std::mutex mtx;
    return mtx;
}

// Normalize family name for lookup (lowercase, strip quotes)
static std::string normalize_family(const std::string& family) {
    std::string result;
    result.reserve(family.size());
    for (char c : family) {
        if (c == '"' || c == '\'') continue;
        result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

TextRenderer::TextRenderer() = default;
TextRenderer::~TextRenderer() = default;

bool TextRenderer::register_font(const std::string& family_name,
                                  const std::vector<uint8_t>& font_data,
                                  int weight, bool italic) {
    if (family_name.empty() || font_data.empty()) return false;

    CFDataRef data = CFDataCreate(nullptr, font_data.data(),
                                   static_cast<CFIndex>(font_data.size()));
    if (!data) return false;

    CTFontDescriptorRef descriptor = CTFontManagerCreateFontDescriptorFromData(data);
    CFRelease(data);

    if (!descriptor) return false;

    std::string key = normalize_family(family_name);

    std::lock_guard<std::mutex> lock(font_registry_mutex());
    registered_fonts()[key].push_back({descriptor, weight, italic});
    return true;
}

bool TextRenderer::has_registered_font(const std::string& family_name) {
    std::string key = normalize_family(family_name);
    std::lock_guard<std::mutex> lock(font_registry_mutex());
    auto it = registered_fonts().find(key);
    return it != registered_fonts().end() && !it->second.empty();
}

void TextRenderer::clear_registered_fonts() {
    std::lock_guard<std::mutex> lock(font_registry_mutex());
    for (auto& [name, variants] : registered_fonts()) {
        for (auto& v : variants) {
            if (v.descriptor) CFRelease(v.descriptor);
        }
    }
    registered_fonts().clear();
}

// Helper: map a CSS-style font weight (100-900) to a CoreText weight value.
// CoreText weight ranges from roughly -1.0 (ultralight) to +1.0 (heavy).
__attribute__((unused))
static CGFloat ct_weight_from_css(int css_weight) {
    if (css_weight <= 100) return -0.8;   // ultralight
    if (css_weight <= 200) return -0.6;   // thin
    if (css_weight <= 300) return -0.4;   // light
    if (css_weight <= 400) return 0.0;    // regular
    if (css_weight <= 500) return 0.23;   // medium
    if (css_weight <= 600) return 0.3;    // semi-bold
    if (css_weight <= 700) return 0.4;    // bold
    if (css_weight <= 800) return 0.56;   // heavy
    return 0.62;                          // black (900)
}

// Try to create a CTFont from a registered web font for the given family.
// Returns nullptr if no web font is registered for this family.
static CTFontRef create_web_font(const std::string& family, CGFloat font_size,
                                  int desired_weight = 400, bool desired_italic = false) {
    std::string key = normalize_family(family);
    std::lock_guard<std::mutex> lock(font_registry_mutex());
    auto it = registered_fonts().find(key);
    if (it == registered_fonts().end() || it->second.empty()) return nullptr;

    // Find the best matching variant
    CTFontDescriptorRef best = nullptr;
    int best_score = INT_MAX;
    for (auto& v : it->second) {
        int score = 0;
        // Weight distance (if variant has a specified weight)
        if (v.weight > 0) {
            score += std::abs(v.weight - desired_weight);
        }
        // Italic mismatch penalty
        if (v.italic != desired_italic) {
            score += 1000;
        }
        if (score < best_score) {
            best_score = score;
            best = v.descriptor;
        }
    }
    if (!best) best = it->second[0].descriptor;

    CTFontRef font = CTFontCreateWithFontDescriptor(best, font_size, nullptr);
    return font;
}

// Map CSS font-family to a macOS font name
static CFStringRef font_name_for_family(const std::string& family) {
    // Note: web fonts are handled separately via create_web_font(), which is called
    // before this function in render_text() and measure_text_width().
    if (family == "monospace" || family == "Courier" || family == "Courier New" ||
        family == "Menlo" || family == "Monaco") {
        return CFSTR("Menlo");
    }
    if (family == "serif" || family == "Times" || family == "Times New Roman" ||
        family == "Georgia") {
        return CFSTR("Times New Roman");
    }
    // Default: sans-serif
    return CFSTR("Helvetica");
}

// Render a single line of text at (x, y) using CoreText.
// Draws directly into the main buffer by wrapping it in a CGBitmapContext
// with a top-left-origin transform, so CG handles all coordinate math.
void TextRenderer::render_single_line(const std::string& text, float x, float y,
                                       float /*font_size*/, const Color& /*color*/,
                                       uint8_t* buffer, int buffer_width, int buffer_height,
                                       CTFontRef font, CGColorRef cg_color, CGColorSpaceRef colorspace,
                                       float letter_spacing, int text_rendering, int font_kerning) {
    if (text.empty()) return;

    // Create attributed string for this line
    CFStringRef cf_text = CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(text.data()),
        static_cast<CFIndex>(text.size()),
        kCFStringEncodingUTF8, false
    );
    if (!cf_text) return;

    // font-kerning: none (2) disables kerning by setting kern to 0
    bool disable_kerning = (font_kerning == 2);

    CFDictionaryRef attrs;
    CFNumberRef kern_value = nullptr;
    if (letter_spacing != 0 || disable_kerning) {
        CGFloat kern = disable_kerning ? 0.0 : static_cast<CGFloat>(letter_spacing);
        kern_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &kern);
        const void* keys[] = {kCTFontAttributeName, kCTForegroundColorAttributeName, kCTKernAttributeName};
        const void* vals[] = {font, cg_color, kern_value};
        attrs = CFDictionaryCreate(
            kCFAllocatorDefault, keys, vals, 3,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
        );
    } else {
        const void* keys[] = {kCTFontAttributeName, kCTForegroundColorAttributeName};
        const void* vals[] = {font, cg_color};
        attrs = CFDictionaryCreate(
            kCFAllocatorDefault, keys, vals, 2,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
        );
    }

    CFAttributedStringRef attr_str = CFAttributedStringCreate(kCFAllocatorDefault, cf_text, attrs);
    if (!attr_str) {
        CFRelease(attrs);
        CFRelease(cf_text);
        if (kern_value) CFRelease(kern_value);
        return;
    }

    CTLineRef line = CTLineCreateWithAttributedString(attr_str);
    if (!line) {
        CFRelease(attr_str);
        CFRelease(attrs);
        CFRelease(cf_text);
        if (kern_value) CFRelease(kern_value);
        return;
    }

    CGFloat ascent, descent, leading;
    CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

    // Create a CG context wrapping the main pixel buffer.
    // IMPORTANT: Do NOT apply a CTM flip — CoreText mirrors glyphs when
    // the CTM has a negative y-scale.  Instead we manually convert our
    // top-left-origin y to CG's native bottom-left-origin y.
    CGContextRef ctx = CGBitmapContextCreate(
        buffer, static_cast<size_t>(buffer_width), static_cast<size_t>(buffer_height),
        8, static_cast<size_t>(buffer_width) * 4, colorspace,
        kCGImageAlphaPremultipliedLast
    );

    if (!ctx) {
        CFRelease(line);
        CFRelease(attr_str);
        CFRelease(attrs);
        CFRelease(cf_text);
        if (kern_value) CFRelease(kern_value);
        return;
    }

    // Apply text-rendering hints
    // 0=auto, 1=optimizeSpeed (disable smoothing), 2=optimizeLegibility, 3=geometricPrecision
    bool smooth = (text_rendering != 1); // optimizeSpeed disables smoothing
    CGContextSetShouldAntialias(ctx, smooth);
    CGContextSetAllowsAntialiasing(ctx, smooth);
    CGContextSetShouldSmoothFonts(ctx, smooth);
    CGContextSetAllowsFontSmoothing(ctx, smooth);
    CGContextSetTextDrawingMode(ctx, kCGTextFill);

    // Ensure text matrix is identity — on some macOS versions the default
    // text matrix in a CGBitmapContext can be non-identity.
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);

    // Convert from top-left coords to CG bottom-left coords.
    // In our system: y = top of text, baseline = y + ascent.
    // In CG system:  baseline_cg = buffer_height - (y + ascent).
    CGFloat baseline_x = static_cast<CGFloat>(x);
    CGFloat baseline_y = static_cast<CGFloat>(buffer_height) - (static_cast<CGFloat>(y) + ascent);
    CGContextSetTextPosition(ctx, baseline_x, baseline_y);
    CTLineDraw(line, ctx);

    CGContextRelease(ctx);
    CFRelease(line);
    CFRelease(attr_str);
    CFRelease(attrs);
    CFRelease(cf_text);
    if (kern_value) CFRelease(kern_value);
}

// Parse CSS font-feature-settings string like '"smcp" 1, "liga" 0' into CoreText feature array
static CFArrayRef parse_font_features(const std::string& settings) {
    if (settings.empty() || settings == "normal") return nullptr;

    CFMutableArrayRef features = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    // Split by comma
    size_t pos = 0;
    while (pos < settings.size()) {
        // Skip whitespace
        while (pos < settings.size() && (settings[pos] == ' ' || settings[pos] == ',')) pos++;
        if (pos >= settings.size()) break;

        // Find the 4-char tag (in quotes or bare)
        std::string tag;
        if (settings[pos] == '"' || settings[pos] == '\'') {
            char quote = settings[pos++];
            while (pos < settings.size() && settings[pos] != quote) {
                tag += settings[pos++];
            }
            if (pos < settings.size()) pos++; // skip closing quote
        } else {
            while (pos < settings.size() && settings[pos] != ' ' && settings[pos] != ',') {
                tag += settings[pos++];
            }
        }

        // Skip whitespace
        while (pos < settings.size() && settings[pos] == ' ') pos++;

        // Parse value (default 1 if omitted)
        int value = 1;
        if (pos < settings.size() && (settings[pos] == '0' || settings[pos] == '1')) {
            value = settings[pos] - '0';
            pos++;
        } else if (pos < settings.size() && std::isdigit(static_cast<unsigned char>(settings[pos]))) {
            value = 0;
            while (pos < settings.size() && std::isdigit(static_cast<unsigned char>(settings[pos]))) {
                value = value * 10 + (settings[pos] - '0');
                pos++;
            }
        }

        if (tag.size() == 4) {
            // Create a CoreText feature setting using kCTFontOpenTypeFeatureTag/kCTFontOpenTypeFeatureValue
            CFStringRef cf_tag = CFStringCreateWithBytes(
                kCFAllocatorDefault,
                reinterpret_cast<const UInt8*>(tag.data()),
                4, kCFStringEncodingASCII, false);
            if (cf_tag) {
                CFNumberRef cf_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
                const void* keys[] = {kCTFontOpenTypeFeatureTag, kCTFontOpenTypeFeatureValue};
                const void* vals[] = {cf_tag, cf_value};
                CFDictionaryRef feature = CFDictionaryCreate(
                    kCFAllocatorDefault, keys, vals, 2,
                    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFArrayAppendValue(features, feature);
                CFRelease(feature);
                CFRelease(cf_value);
                CFRelease(cf_tag);
            }
        }
    }

    if (CFArrayGetCount(features) == 0) {
        CFRelease(features);
        return nullptr;
    }
    return features;
}

// Parse CSS font-variation-settings like '"wght" 600, "wdth" 75' into CoreText variation dict
static CFDictionaryRef parse_font_variations(const std::string& settings) {
    if (settings.empty() || settings == "normal") return nullptr;

    CFMutableDictionaryRef variations = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    size_t pos = 0;
    while (pos < settings.size()) {
        while (pos < settings.size() && (settings[pos] == ' ' || settings[pos] == ',')) pos++;
        if (pos >= settings.size()) break;

        // Parse 4-char axis tag
        std::string tag;
        if (settings[pos] == '"' || settings[pos] == '\'') {
            char quote = settings[pos++];
            while (pos < settings.size() && settings[pos] != quote) {
                tag += settings[pos++];
            }
            if (pos < settings.size()) pos++;
        } else {
            while (pos < settings.size() && settings[pos] != ' ' && settings[pos] != ',') {
                tag += settings[pos++];
            }
        }

        while (pos < settings.size() && settings[pos] == ' ') pos++;

        // Parse numeric value
        float value = 0;
        bool negative = false;
        if (pos < settings.size() && settings[pos] == '-') { negative = true; pos++; }
        while (pos < settings.size() && (std::isdigit(static_cast<unsigned char>(settings[pos])) || settings[pos] == '.')) {
            if (settings[pos] == '.') {
                pos++;
                float frac = 0.1f;
                while (pos < settings.size() && std::isdigit(static_cast<unsigned char>(settings[pos]))) {
                    value += (settings[pos] - '0') * frac;
                    frac *= 0.1f;
                    pos++;
                }
            } else {
                value = value * 10 + (settings[pos] - '0');
                pos++;
            }
        }
        if (negative) value = -value;

        if (tag.size() == 4) {
            // Create 4-byte tag number (big-endian)
            uint32_t tag_num = (static_cast<uint32_t>(tag[0]) << 24) |
                               (static_cast<uint32_t>(tag[1]) << 16) |
                               (static_cast<uint32_t>(tag[2]) << 8) |
                               static_cast<uint32_t>(tag[3]);
            CFNumberRef cf_tag = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &tag_num);
            CFNumberRef cf_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &value);
            CFDictionarySetValue(variations, cf_tag, cf_value);
            CFRelease(cf_value);
            CFRelease(cf_tag);
        }
    }

    if (CFDictionaryGetCount(variations) == 0) {
        CFRelease(variations);
        return nullptr;
    }
    return variations;
}

void TextRenderer::render_text(const std::string& text, float x, float y,
                                float font_size, const Color& color,
                                uint8_t* buffer, int buffer_width, int buffer_height,
                                const std::string& font_family,
                                int font_weight, bool font_italic,
                                float letter_spacing,
                                const std::string& font_feature_settings,
                                const std::string& font_variation_settings,
                                int text_rendering, int font_kerning,
                                int font_optical_sizing) {
    (void)font_optical_sizing;
    if (text.empty() || buffer_width <= 0 || buffer_height <= 0) return;
    if (!buffer) return;

    if (font_size < 1.0f) font_size = 1.0f;

    // Try registered web fonts first, then fall back to system fonts
    CTFontRef base_font = create_web_font(font_family, static_cast<CGFloat>(font_size),
                                            font_weight, font_italic);
    if (!base_font) {
        CFStringRef ct_font_name = font_name_for_family(font_family);
        base_font = CTFontCreateWithName(ct_font_name, static_cast<CGFloat>(font_size), nullptr);
    }
    if (!base_font) {
        base_font = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, static_cast<CGFloat>(font_size), nullptr);
    }
    if (!base_font) return;

    // Apply bold and italic traits (only for system fonts — web fonts already have the right variant)
    CTFontRef font = base_font;
    bool is_web_font = has_registered_font(font_family);
    if (!is_web_font) {
        CTFontSymbolicTraits traits = 0;
        if (font_weight >= 700) traits |= kCTFontBoldTrait;
        if (font_italic) traits |= kCTFontItalicTrait;

        if (traits != 0) {
            CTFontRef styled_font = CTFontCreateCopyWithSymbolicTraits(base_font, 0.0, nullptr, traits, traits);
            if (styled_font) {
                CFRelease(base_font);
                font = styled_font;
            }
            // If trait application fails, fall back to base_font
        }
    }

    // Apply font-feature-settings via CoreText font descriptor
    if (!font_feature_settings.empty()) {
        CFArrayRef features = parse_font_features(font_feature_settings);
        if (features) {
            const void* desc_keys[] = {kCTFontFeatureSettingsAttribute};
            const void* desc_vals[] = {features};
            CFDictionaryRef desc_attrs = CFDictionaryCreate(
                kCFAllocatorDefault, desc_keys, desc_vals, 1,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes(desc_attrs);
            CTFontRef featured_font = CTFontCreateCopyWithAttributes(font, 0.0, nullptr, descriptor);
            if (featured_font) {
                CFRelease(font);
                font = featured_font;
            }
            CFRelease(descriptor);
            CFRelease(desc_attrs);
            CFRelease(features);
        }
    }

    // Apply font-variation-settings via CoreText variation axes
    if (!font_variation_settings.empty()) {
        CFDictionaryRef variations = parse_font_variations(font_variation_settings);
        if (variations) {
            const void* desc_keys[] = {kCTFontVariationAttribute};
            const void* desc_vals[] = {variations};
            CFDictionaryRef desc_attrs = CFDictionaryCreate(
                kCFAllocatorDefault, desc_keys, desc_vals, 1,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes(desc_attrs);
            CTFontRef var_font = CTFontCreateCopyWithAttributes(font, 0.0, nullptr, descriptor);
            if (var_font) {
                CFRelease(font);
                font = var_font;
            }
            CFRelease(descriptor);
            CFRelease(desc_attrs);
            CFRelease(variations);
        }
    }

    CGFloat r = static_cast<CGFloat>(color.r) / 255.0;
    CGFloat g = static_cast<CGFloat>(color.g) / 255.0;
    CGFloat b = static_cast<CGFloat>(color.b) / 255.0;
    CGFloat a = static_cast<CGFloat>(color.a) / 255.0;

    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGFloat components[4] = {r, g, b, a};
    CGColorRef cg_color = CGColorCreate(colorspace, components);

    // Handle multi-line text (newlines)
    float line_height = font_size * 1.2f;
    float current_y = y;
    std::string::size_type pos = 0;
    std::string::size_type prev = 0;
    bool has_newlines = (text.find('\n') != std::string::npos);

    if (has_newlines) {
        while (pos <= text.size()) {
            pos = text.find('\n', prev);
            if (pos == std::string::npos) pos = text.size();

            std::string line_text = text.substr(prev, pos - prev);
            if (!line_text.empty()) {
                render_single_line(line_text, x, current_y, font_size, color,
                                   buffer, buffer_width, buffer_height,
                                   font, cg_color, colorspace, letter_spacing,
                                   text_rendering, font_kerning);
            }
            current_y += line_height;
            prev = pos + 1;
            if (pos == text.size()) break;
        }
    } else {
        render_single_line(text, x, y, font_size, color,
                           buffer, buffer_width, buffer_height,
                           font, cg_color, colorspace, letter_spacing,
                           text_rendering, font_kerning);
    }

    CGColorRelease(cg_color);
    CGColorSpaceRelease(colorspace);
    CFRelease(font);
}

float TextRenderer::measure_text_width(const std::string& text, float font_size,
                                       const std::string& font_family) {
    if (text.empty()) return 0.0f;
    if (font_size < 1.0f) font_size = 1.0f;

    // Try registered web fonts first
    CTFontRef font = create_web_font(font_family, static_cast<CGFloat>(font_size));
    if (!font) {
        CFStringRef ct_font_name = font_name_for_family(font_family);
        font = CTFontCreateWithName(ct_font_name, static_cast<CGFloat>(font_size), nullptr);
    }
    if (!font) return 0.0f;

    CFStringRef cf_text = CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(text.data()),
        static_cast<CFIndex>(text.size()),
        kCFStringEncodingUTF8,
        false
    );
    if (!cf_text) {
        CFRelease(font);
        return 0.0f;
    }

    const void* keys[] = {kCTFontAttributeName};
    const void* vals[] = {font};
    CFDictionaryRef attrs = CFDictionaryCreate(
        kCFAllocatorDefault, keys, vals, 1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );

    CFAttributedStringRef attr_str = CFAttributedStringCreate(kCFAllocatorDefault, cf_text, attrs);
    if (!attr_str) {
        CFRelease(attrs);
        CFRelease(cf_text);
        CFRelease(font);
        return 0.0f;
    }

    CTLineRef line = CTLineCreateWithAttributedString(attr_str);
    if (!line) {
        CFRelease(attr_str);
        CFRelease(attrs);
        CFRelease(cf_text);
        CFRelease(font);
        return 0.0f;
    }

    CGFloat ascent, descent, leading;
    double width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

    CFRelease(line);
    CFRelease(attr_str);
    CFRelease(attrs);
    CFRelease(cf_text);
    CFRelease(font);

    return static_cast<float>(width);
}

float TextRenderer::measure_text_width(const std::string& text, float font_size,
                                        const std::string& font_family,
                                        int font_weight, bool font_italic,
                                        float letter_spacing) {
    if (text.empty()) return 0.0f;
    if (font_size < 1.0f) font_size = 1.0f;

    // Try registered web fonts first
    CTFontRef base_font = create_web_font(font_family, static_cast<CGFloat>(font_size),
                                            font_weight, font_italic);
    if (!base_font) {
        CFStringRef ct_font_name = font_name_for_family(font_family);
        base_font = CTFontCreateWithName(ct_font_name, static_cast<CGFloat>(font_size), nullptr);
    }
    if (!base_font) {
        base_font = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, static_cast<CGFloat>(font_size), nullptr);
    }
    if (!base_font) return 0.0f;

    // Apply bold and italic traits (only for system fonts)
    CTFontRef font = base_font;
    bool is_web_font = has_registered_font(font_family);
    if (!is_web_font) {
        CTFontSymbolicTraits traits = 0;
        if (font_weight >= 700) traits |= kCTFontBoldTrait;
        if (font_italic) traits |= kCTFontItalicTrait;

        if (traits != 0) {
            CTFontRef styled_font = CTFontCreateCopyWithSymbolicTraits(base_font, 0.0, nullptr, traits, traits);
            if (styled_font) {
                CFRelease(base_font);
                font = styled_font;
            }
        }
    }

    // Create attributed string
    CFStringRef cf_text = CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(text.data()),
        static_cast<CFIndex>(text.size()),
        kCFStringEncodingUTF8,
        false
    );
    if (!cf_text) {
        CFRelease(font);
        return 0.0f;
    }

    // Build attributes with optional kern (letter_spacing)
    CFDictionaryRef attrs;
    CFNumberRef kern_value = nullptr;
    if (letter_spacing != 0) {
        CGFloat kern = static_cast<CGFloat>(letter_spacing);
        kern_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &kern);
        const void* keys[] = {kCTFontAttributeName, kCTKernAttributeName};
        const void* vals[] = {font, kern_value};
        attrs = CFDictionaryCreate(
            kCFAllocatorDefault, keys, vals, 2,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
        );
    } else {
        const void* keys[] = {kCTFontAttributeName};
        const void* vals[] = {font};
        attrs = CFDictionaryCreate(
            kCFAllocatorDefault, keys, vals, 1,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
        );
    }

    CFAttributedStringRef attr_str = CFAttributedStringCreate(kCFAllocatorDefault, cf_text, attrs);
    if (!attr_str) {
        CFRelease(attrs);
        CFRelease(cf_text);
        CFRelease(font);
        if (kern_value) CFRelease(kern_value);
        return 0.0f;
    }

    CTLineRef line = CTLineCreateWithAttributedString(attr_str);
    if (!line) {
        CFRelease(attr_str);
        CFRelease(attrs);
        CFRelease(cf_text);
        CFRelease(font);
        if (kern_value) CFRelease(kern_value);
        return 0.0f;
    }

    CGFloat ascent, descent, leading;
    double width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

    CFRelease(line);
    CFRelease(attr_str);
    CFRelease(attrs);
    CFRelease(cf_text);
    CFRelease(font);
    if (kern_value) CFRelease(kern_value);

    return static_cast<float>(width);
}

} // namespace clever::paint
