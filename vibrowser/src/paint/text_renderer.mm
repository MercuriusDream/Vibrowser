#include <clever/paint/text_renderer.h>
#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <mutex>

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>

namespace {
constexpr size_t kTextWidthCacheMaxEntries = 512;
}

namespace clever::paint {

namespace {

// ---- Web font registry ----
// Maps normalized CSS font-family to registered CoreGraphics fonts, preserving
// variant metadata for weight/style matching.
struct RegisteredWebFont {
    CGFontRef graphics_font = nullptr;
    int weight = 0;
    bool italic = false;
};

std::unordered_map<std::string, std::vector<RegisteredWebFont>>& registered_fonts() {
    static std::unordered_map<std::string, std::vector<RegisteredWebFont>> fonts;
    return fonts;
}

std::mutex& font_registry_mutex() {
    static std::mutex mtx;
    return mtx;
}

std::atomic<uint64_t>& font_registry_generation() {
    static std::atomic<uint64_t> generation{0};
    return generation;
}

struct TextWidthCacheKey {
    std::string text;
    float font_size = 0.0f;
    std::string font_family;
    int font_weight = 400;
    bool font_italic = false;
    float letter_spacing = 0.0f;
    float word_spacing = 0.0f;

    bool operator==(const TextWidthCacheKey& other) const {
        return text == other.text &&
               font_size == other.font_size &&
               font_family == other.font_family &&
               font_weight == other.font_weight &&
               font_italic == other.font_italic &&
               letter_spacing == other.letter_spacing &&
               word_spacing == other.word_spacing;
    }
};

struct TextWidthCacheKeyHash {
    size_t operator()(const TextWidthCacheKey& key) const {
        size_t seed = std::hash<std::string>{}(key.text);
        auto mix = [&seed](size_t value) {
            seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };
        mix(std::hash<float>{}(key.font_size));
        mix(std::hash<std::string>{}(key.font_family));
        mix(std::hash<int>{}(key.font_weight));
        mix(std::hash<bool>{}(key.font_italic));
        mix(std::hash<float>{}(key.letter_spacing));
        mix(std::hash<float>{}(key.word_spacing));
        return seed;
    }
};

struct TextWidthCacheState {
    std::unordered_map<TextWidthCacheKey, float, TextWidthCacheKeyHash> entries;
    std::vector<TextWidthCacheKey> insertion_order;
    uint64_t hits = 0;
    uint64_t misses = 0;
};

TextWidthCacheState& text_width_cache_state() {
    static TextWidthCacheState state;
    return state;
}

std::mutex& text_width_cache_mutex() {
    static std::mutex mtx;
    return mtx;
}

void invalidate_text_width_cache_locked() {
    auto& cache = text_width_cache_state();
    cache.entries.clear();
    cache.insertion_order.clear();
}

struct TextLineLayoutCacheKey {
    std::string text;
    std::string font_descriptor;
    float kern = 0.0f;
    uint64_t font_generation = 0;

    bool operator==(const TextLineLayoutCacheKey& other) const {
        return text == other.text &&
               font_descriptor == other.font_descriptor &&
               kern == other.kern &&
               font_generation == other.font_generation;
    }
};

struct TextLineLayoutCacheKeyHash {
    size_t operator()(const TextLineLayoutCacheKey& key) const {
        size_t seed = std::hash<std::string>{}(key.text);
        auto mix = [&seed](size_t value) {
            seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };
        mix(std::hash<std::string>{}(key.font_descriptor));
        mix(std::hash<float>{}(key.kern));
        mix(std::hash<uint64_t>{}(key.font_generation));
        return seed;
    }
};

struct CachedTextLineLayout {
    CTLineRef line = nullptr;
    CGFloat ascent = 0.0;
};

struct TextLineLayoutCacheState {
    std::unordered_map<TextLineLayoutCacheKey, CachedTextLineLayout, TextLineLayoutCacheKeyHash> entries;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t creations = 0;

    ~TextLineLayoutCacheState() {
        for (auto& [key, entry] : entries) {
            if (entry.line) {
                CFRelease(entry.line);
            }
        }
    }
};

TextLineLayoutCacheState& text_line_layout_cache_state() {
    static TextLineLayoutCacheState state;
    return state;
}

std::mutex& text_line_layout_cache_mutex() {
    static std::mutex mtx;
    return mtx;
}

void invalidate_text_line_layout_cache_locked() {
    auto& cache = text_line_layout_cache_state();
    for (auto& [key, entry] : cache.entries) {
        if (entry.line) {
            CFRelease(entry.line);
        }
    }
    cache.entries.clear();
}

std::string cf_string_to_std_string(CFStringRef value) {
    if (!value) return {};

    CFIndex length = CFStringGetLength(value);
    CFIndex max_bytes = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::string out(static_cast<size_t>(max_bytes), '\0');
    if (!CFStringGetCString(value, out.data(), max_bytes, kCFStringEncodingUTF8)) {
        return {};
    }
    out.resize(std::strlen(out.c_str()));
    return out;
}

std::string font_layout_descriptor_token(CTFontRef font) {
    if (!font) return {};

    if (CFStringRef postscript_name = CTFontCopyPostScriptName(font)) {
        std::string token = cf_string_to_std_string(postscript_name);
        CFRelease(postscript_name);
        if (CFStringRef full_name = CTFontCopyFullName(font)) {
            token += "|";
            token += cf_string_to_std_string(full_name);
            CFRelease(full_name);
        }
        token += "|" + std::to_string(CTFontGetSize(font));
        token += "|" + std::to_string(CTFontGetAscent(font));
        token += "|" + std::to_string(CTFontGetDescent(font));
        token += "|" + std::to_string(CTFontGetLeading(font));
        token += "|" + std::to_string(CTFontGetSlantAngle(font));
        token += "|" + std::to_string(CTFontGetCapHeight(font));
        token += "|" + std::to_string(CTFontGetXHeight(font));
        return token;
    }

    std::string token;
    if (CFStringRef full_name = CTFontCopyFullName(font)) {
        token += cf_string_to_std_string(full_name);
        CFRelease(full_name);
    }
    token += "|" + std::to_string(CTFontGetSize(font));
    token += "|" + std::to_string(CTFontGetAscent(font));
    token += "|" + std::to_string(CTFontGetDescent(font));
    token += "|" + std::to_string(CTFontGetLeading(font));
    token += "|" + std::to_string(CTFontGetSlantAngle(font));
    token += "|" + std::to_string(CTFontGetCapHeight(font));
    token += "|" + std::to_string(CTFontGetXHeight(font));
    return token;
}

// Normalize family name for lookup (lowercase, strip quotes)
std::string normalize_family(const std::string& family) {
    std::string result;
    result.reserve(family.size());
    for (char c : family) {
        if (c == '"' || c == '\'') continue;
        result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

} // namespace

TextRenderer::TextRenderer() = default;
TextRenderer::~TextRenderer() = default;

bool TextRenderer::register_font(const std::string& family_name,
                                  const std::vector<uint8_t>& font_data,
                                  int weight, bool italic) {
    if (family_name.empty() || font_data.empty()) return false;

    std::string key = normalize_family(family_name);
    {
        std::lock_guard<std::mutex> lock(font_registry_mutex());
        auto it = registered_fonts().find(key);
        if (it != registered_fonts().end()) {
            for (const auto& v : it->second) {
                if (v.weight == weight && v.italic == italic) {
                    return true;
                }
            }
        }
    }

    size_t byte_count = font_data.size();
    UInt8* owned_bytes = static_cast<UInt8*>(std::malloc(byte_count));
    if (!owned_bytes) return false;
    std::memcpy(owned_bytes, font_data.data(), byte_count);

    CFDataRef data = CFDataCreateWithBytesNoCopy(
        kCFAllocatorDefault,
        owned_bytes,
        static_cast<CFIndex>(byte_count),
        kCFAllocatorMalloc
    );
    if (!data) {
        std::free(owned_bytes);
        return false;
    }

    CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
    CFRelease(data);
    if (!provider) return false;

    CGFontRef graphics_font = CGFontCreateWithDataProvider(provider);
    CGDataProviderRelease(provider);
    if (!graphics_font) return false;

    CFErrorRef error = nullptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    bool ok = CTFontManagerRegisterGraphicsFont(graphics_font, &error);
#pragma clang diagnostic pop
    bool already_registered = false;
    if (!ok && error) {
        if (CFEqual(CFErrorGetDomain(error), kCTFontManagerErrorDomain) &&
            CFErrorGetCode(error) == kCTFontManagerErrorAlreadyRegistered) {
            already_registered = true;
        }
        CFRelease(error);
    }
    if (!ok && !already_registered) {
        CFRelease(graphics_font);
        return false;
    }

    std::lock_guard<std::mutex> lock(font_registry_mutex());
    auto& variants = registered_fonts()[key];
    for (const auto& v : variants) {
        if (v.weight == weight && v.italic == italic) {
            CFRelease(graphics_font);
            return true;
        }
    }
    variants.push_back({graphics_font, weight, italic});
    {
        std::lock_guard<std::mutex> cache_lock(text_width_cache_mutex());
        invalidate_text_width_cache_locked();
    }
    {
        std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
        invalidate_text_line_layout_cache_locked();
    }
    font_registry_generation().fetch_add(1, std::memory_order_relaxed);
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
            if (!v.graphics_font) continue;
            CFErrorRef error = nullptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            CTFontManagerUnregisterGraphicsFont(v.graphics_font, &error);
#pragma clang diagnostic pop
            if (error) CFRelease(error);
            CFRelease(v.graphics_font);
        }
    }
    registered_fonts().clear();
    {
        std::lock_guard<std::mutex> cache_lock(text_width_cache_mutex());
        invalidate_text_width_cache_locked();
    }
    {
        std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
        invalidate_text_line_layout_cache_locked();
    }
    font_registry_generation().fetch_add(1, std::memory_order_relaxed);
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
    CGFontRef best = nullptr;
    int best_score = INT_MAX;
    for (auto& v : it->second) {
        if (!v.graphics_font) continue;
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
            best = v.graphics_font;
        }
    }
    if (!best) {
        for (auto& v : it->second) {
            if (v.graphics_font) {
                best = v.graphics_font;
                break;
            }
        }
    }
    if (!best) return nullptr;

    CTFontRef font = CTFontCreateWithGraphicsFont(best, font_size, nullptr, nullptr);
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
                                       float letter_spacing, int text_rendering, int font_kerning,
                                       float word_spacing, float clip_x, float clip_y,
                                       float clip_w, float clip_h) {
    if (text.empty()) return;

    // font-kerning: none (2) disables kerning by setting kern to 0
    bool disable_kerning = (font_kerning == 2);
    CGFloat effective_kern = disable_kerning ? 0.0 : static_cast<CGFloat>(letter_spacing);

    if (word_spacing != 0) {
        CFStringRef cf_text = CFStringCreateWithBytes(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(text.data()),
            static_cast<CFIndex>(text.size()),
            kCFStringEncodingUTF8, false
        );
        if (!cf_text) return;

        CFDictionaryRef attrs;
        CFNumberRef kern_value = nullptr;
        if (letter_spacing != 0 || disable_kerning) {
            kern_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &effective_kern);
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

        if (!attrs) {
            CFRelease(cf_text);
            if (kern_value) CFRelease(kern_value);
            return;
        }

        CFAttributedStringRef attr_str = CFAttributedStringCreate(kCFAllocatorDefault, cf_text, attrs);
        if (!attr_str) {
            CFRelease(attrs);
            CFRelease(cf_text);
            if (kern_value) CFRelease(kern_value);
            return;
        }

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
            CFRelease(attr_str);
            CFRelease(attrs);
            CFRelease(cf_text);
            if (kern_value) CFRelease(kern_value);
            return;
        }

        if (clip_x >= 0 && clip_w > 0 && clip_h > 0) {
            float cg_clip_y = static_cast<float>(buffer_height) - (clip_y + clip_h);
            CGContextClipToRect(ctx, CGRectMake(clip_x, cg_clip_y, clip_w, clip_h));
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

        float cursor_x = x;
        size_t i = 0;
        while (i < text.size()) {
            // Find next space
            size_t sp = text.find(' ', i);
            if (sp == std::string::npos) sp = text.size();
            std::string word = text.substr(i, sp - i);

            if (!word.empty()) {
                // Render the word
                CFStringRef word_cf = CFStringCreateWithBytes(
                    kCFAllocatorDefault,
                    reinterpret_cast<const UInt8*>(word.data()),
                    static_cast<CFIndex>(word.size()),
                    kCFStringEncodingUTF8, false);
                if (word_cf) {
                    CFAttributedStringRef word_attr_str = CFAttributedStringCreate(kCFAllocatorDefault, word_cf, attrs);
                    if (word_attr_str) {
                        CTLineRef word_line = CTLineCreateWithAttributedString(word_attr_str);
                        if (word_line) {
                            CGFloat ascent, descent, leading;
                            double width = CTLineGetTypographicBounds(word_line, &ascent, &descent, &leading);
                            CGFloat baseline_y = static_cast<CGFloat>(buffer_height) - (static_cast<CGFloat>(y) + ascent);
                            CGContextSetTextPosition(ctx, cursor_x, baseline_y);
                            CTLineDraw(word_line, ctx);
                            cursor_x += width;
                            CFRelease(word_line);
                        }
                        CFRelease(word_attr_str);
                    }
                    CFRelease(word_cf);
                }
            }

            // Handle space with word_spacing
            if (sp < text.size()) {
                // Measure space width using a single space in attrs
                CFStringRef space_cf = CFStringCreateWithCString(kCFAllocatorDefault, " ", kCFStringEncodingUTF8);
                if (space_cf) {
                    CFAttributedStringRef space_attr_str = CFAttributedStringCreate(kCFAllocatorDefault, space_cf, attrs);
                    if (space_attr_str) {
                        CTLineRef space_line = CTLineCreateWithAttributedString(space_attr_str);
                        if (space_line) {
                            CGFloat ascent_sp, descent_sp, leading_sp;
                            double space_width = CTLineGetTypographicBounds(space_line, &ascent_sp, &descent_sp, &leading_sp);
                            cursor_x += space_width + word_spacing;
                            CFRelease(space_line);
                        }
                        CFRelease(space_attr_str);
                    }
                    CFRelease(space_cf);
                }
            }

            i = sp + 1;
            if (sp == text.size()) break;
        }

        CGContextRelease(ctx);
        CFRelease(attr_str);
        CFRelease(attrs);
        CFRelease(cf_text);
        if (kern_value) CFRelease(kern_value);
        return;
    }

    CTLineRef line = nullptr;
    CGFloat ascent = 0.0;
    TextLineLayoutCacheKey cache_key{
        text,
        font_layout_descriptor_token(font),
        static_cast<float>(effective_kern),
        font_registry_generation().load(std::memory_order_relaxed)
    };
    {
        std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
        auto& cache = text_line_layout_cache_state();
        auto it = cache.entries.find(cache_key);
        if (it != cache.entries.end()) {
            line = static_cast<CTLineRef>(CFRetain(it->second.line));
            ascent = it->second.ascent;
            ++cache.hits;
        } else {
            ++cache.misses;
        }
    }

    if (!line) {
        CFStringRef cf_text = CFStringCreateWithBytes(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(text.data()),
            static_cast<CFIndex>(text.size()),
            kCFStringEncodingUTF8, false
        );
        if (!cf_text) return;

        CFDictionaryRef attrs;
        CFNumberRef kern_value = nullptr;
        if (letter_spacing != 0 || disable_kerning) {
            kern_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &effective_kern);
            const void* keys[] = {kCTFontAttributeName, kCTKernAttributeName,
                                  kCTForegroundColorFromContextAttributeName};
            const void* vals[] = {font, kern_value, kCFBooleanTrue};
            attrs = CFDictionaryCreate(
                kCFAllocatorDefault, keys, vals, 3,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
            );
        } else {
            const void* keys[] = {kCTFontAttributeName, kCTForegroundColorFromContextAttributeName};
            const void* vals[] = {font, kCFBooleanTrue};
            attrs = CFDictionaryCreate(
                kCFAllocatorDefault, keys, vals, 2,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
            );
        }

        if (!attrs) {
            CFRelease(cf_text);
            if (kern_value) CFRelease(kern_value);
            return;
        }

        CFAttributedStringRef attr_str = CFAttributedStringCreate(kCFAllocatorDefault, cf_text, attrs);
        if (!attr_str) {
            CFRelease(attrs);
            CFRelease(cf_text);
            if (kern_value) CFRelease(kern_value);
            return;
        }

        line = CTLineCreateWithAttributedString(attr_str);
        if (line) {
            CGFloat descent = 0.0;
            CGFloat leading = 0.0;
            CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
            std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
            auto& cache = text_line_layout_cache_state();
            auto [it, inserted] = cache.entries.emplace(cache_key, CachedTextLineLayout{});
            if (inserted) {
                it->second.line = static_cast<CTLineRef>(CFRetain(line));
                it->second.ascent = ascent;
                ++cache.creations;
            }
        }

        CFRelease(attr_str);
        CFRelease(attrs);
        CFRelease(cf_text);
        if (kern_value) CFRelease(kern_value);
        if (!line) return;
    }

    CGContextRef ctx = CGBitmapContextCreate(
        buffer, static_cast<size_t>(buffer_width), static_cast<size_t>(buffer_height),
        8, static_cast<size_t>(buffer_width) * 4, colorspace,
        kCGImageAlphaPremultipliedLast
    );
    if (!ctx) {
        CFRelease(line);
        return;
    }

    if (clip_x >= 0 && clip_w > 0 && clip_h > 0) {
        float cg_clip_y = static_cast<float>(buffer_height) - (clip_y + clip_h);
        CGContextClipToRect(ctx, CGRectMake(clip_x, cg_clip_y, clip_w, clip_h));
    }

    bool smooth = (text_rendering != 1);
    CGContextSetShouldAntialias(ctx, smooth);
    CGContextSetAllowsAntialiasing(ctx, smooth);
    CGContextSetShouldSmoothFonts(ctx, smooth);
    CGContextSetAllowsFontSmoothing(ctx, smooth);
    CGContextSetTextDrawingMode(ctx, kCGTextFill);
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    CGContextSetFillColorWithColor(ctx, cg_color);

    CGFloat baseline_x = static_cast<CGFloat>(x);
    CGFloat baseline_y = static_cast<CGFloat>(buffer_height) - (static_cast<CGFloat>(y) + ascent);
    CGContextSetTextPosition(ctx, baseline_x, baseline_y);
    CTLineDraw(line, ctx);

    CGContextRelease(ctx);
    CFRelease(line);
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
                                int font_optical_sizing, float word_spacing,
                                float clip_x, float clip_y, float clip_w, float clip_h) {
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
    bool is_web_font = TextRenderer::has_registered_font(font_family);
    if (!is_web_font) {
        CTFontSymbolicTraits traits = 0;
        if (font_weight >= 600) traits |= kCTFontBoldTrait;
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
                                   text_rendering, font_kerning, word_spacing,
                                   clip_x, clip_y, clip_w, clip_h);
            }
            current_y += line_height;
            prev = pos + 1;
            if (pos == text.size()) break;
        }
    } else {
        render_single_line(text, x, y, font_size, color,
                           buffer, buffer_width, buffer_height,
                           font, cg_color, colorspace, letter_spacing,
                           text_rendering, font_kerning, word_spacing,
                           clip_x, clip_y, clip_w, clip_h);
    }

    CGColorRelease(cg_color);
    CGColorSpaceRelease(colorspace);
    CFRelease(font);
}

float measure_text_width_uncached(const std::string& text, float font_size,
                                  const std::string& font_family,
                                  int font_weight, bool font_italic,
                                  float letter_spacing, float word_spacing) {
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

    CTFontRef font = base_font;
    bool is_web_font = TextRenderer::has_registered_font(font_family);
    if (!is_web_font) {
        CTFontSymbolicTraits traits = 0;
        if (font_weight >= 600) traits |= kCTFontBoldTrait;
        if (font_italic) traits |= kCTFontItalicTrait;

        if (traits != 0) {
            CTFontRef styled_font = CTFontCreateCopyWithSymbolicTraits(base_font, 0.0, nullptr, traits, traits);
            if (styled_font) {
                CFRelease(base_font);
                font = styled_font;
            }
        }
    }

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

    if (word_spacing != 0) {
        int space_count = 0;
        for (char c : text) {
            if (c == ' ') space_count++;
        }
        width += space_count * word_spacing;
    }

    return static_cast<float>(width);
}

float TextRenderer::measure_text_width(const std::string& text, float font_size,
                                       const std::string& font_family) {
    return measure_text_width(text, font_size, font_family, 400, false, 0.0f, 0.0f);
}

float TextRenderer::measure_text_width(const std::string& text, float font_size,
                                        const std::string& font_family,
                                        int font_weight, bool font_italic,
                                        float letter_spacing, float word_spacing) {
    if (text.empty()) return 0.0f;
    if (font_size < 1.0f) font_size = 1.0f;
    TextWidthCacheKey key{text, font_size, normalize_family(font_family),
                          font_weight, font_italic, letter_spacing, word_spacing};
    {
        std::lock_guard<std::mutex> cache_lock(text_width_cache_mutex());
        auto& cache = text_width_cache_state();
        auto it = cache.entries.find(key);
        if (it != cache.entries.end()) {
            ++cache.hits;
            return it->second;
        }
        ++cache.misses;
    }

    float width = measure_text_width_uncached(text, font_size, font_family,
                                              font_weight, font_italic,
                                              letter_spacing, word_spacing);

    std::lock_guard<std::mutex> cache_lock(text_width_cache_mutex());
    auto& cache = text_width_cache_state();
    if (cache.entries.size() >= kTextWidthCacheMaxEntries && !cache.insertion_order.empty()) {
        cache.entries.erase(cache.insertion_order.front());
        cache.insertion_order.erase(cache.insertion_order.begin());
    }
    cache.insertion_order.push_back(key);
    cache.entries.emplace(std::move(key), width);
    return width;
}

void reset_text_width_cache_stats_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_width_cache_mutex());
    auto& cache = text_width_cache_state();
    cache.hits = 0;
    cache.misses = 0;
}

size_t text_width_cache_size_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_width_cache_mutex());
    return text_width_cache_state().entries.size();
}

uint64_t text_width_cache_hit_count_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_width_cache_mutex());
    return text_width_cache_state().hits;
}

uint64_t text_width_cache_miss_count_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_width_cache_mutex());
    return text_width_cache_state().misses;
}

void reset_text_line_layout_cache_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
    auto& cache = text_line_layout_cache_state();
    invalidate_text_line_layout_cache_locked();
    cache.hits = 0;
    cache.misses = 0;
    cache.creations = 0;
}

size_t text_line_layout_cache_size_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
    return text_line_layout_cache_state().entries.size();
}

uint64_t text_line_layout_cache_hit_count_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
    return text_line_layout_cache_state().hits;
}

uint64_t text_line_layout_cache_miss_count_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
    return text_line_layout_cache_state().misses;
}

uint64_t text_line_layout_creation_count_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
    return text_line_layout_cache_state().creations;
}

} // namespace clever::paint
