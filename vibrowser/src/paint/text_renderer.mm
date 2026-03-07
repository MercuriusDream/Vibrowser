#include <clever/paint/text_renderer.h>
#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <list>
#include <mutex>

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>

namespace {
constexpr size_t kTextWidthCacheMaxEntries = 512;
constexpr size_t kTextLineLayoutCacheDefaultMaxEntries = 256;
constexpr size_t kTextRunRasterCacheDefaultMaxEntries = 256;
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
    uint64_t font_generation = 0;

    bool operator==(const TextWidthCacheKey& other) const {
        return text == other.text &&
               font_size == other.font_size &&
               font_family == other.font_family &&
               font_weight == other.font_weight &&
               font_italic == other.font_italic &&
               letter_spacing == other.letter_spacing &&
               word_spacing == other.word_spacing &&
               font_generation == other.font_generation;
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
        mix(std::hash<uint64_t>{}(key.font_generation));
        return seed;
    }
};

struct CachedTextWidth {
    float width = 0.0f;
    std::list<TextWidthCacheKey>::iterator lru_it{};
};

struct TextWidthCacheState {
    std::unordered_map<TextWidthCacheKey, CachedTextWidth, TextWidthCacheKeyHash> entries;
    std::list<TextWidthCacheKey> lru_order;
    size_t max_entries = kTextWidthCacheMaxEntries;
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
    cache.lru_order.clear();
}

void text_width_cache_touch_locked(CachedTextWidth& entry) {
    auto& cache = text_width_cache_state();
    if (entry.lru_it == cache.lru_order.end() || cache.lru_order.empty()) {
        return;
    }

    auto newest_it = cache.lru_order.end();
    --newest_it;
    if (entry.lru_it == newest_it) {
        return;
    }

    cache.lru_order.splice(cache.lru_order.end(), cache.lru_order, entry.lru_it);
}

void text_width_cache_evict_locked() {
    auto& cache = text_width_cache_state();
    while (cache.entries.size() > cache.max_entries && !cache.lru_order.empty()) {
        auto evict_it = cache.lru_order.begin();
        auto entry_it = cache.entries.find(*evict_it);
        cache.lru_order.erase(evict_it);
        if (entry_it == cache.entries.end()) {
            continue;
        }
        cache.entries.erase(entry_it);
    }
}

std::string text_width_cache_key_for_testing(const TextWidthCacheKey& key) {
    return key.text + "|family=" + key.font_family +
           "|weight=" + std::to_string(key.font_weight) +
           "|italic=" + std::to_string(key.font_italic ? 1 : 0) +
           "|size=" + std::to_string(key.font_size) +
           "|letter=" + std::to_string(key.letter_spacing) +
           "|word=" + std::to_string(key.word_spacing) +
           "|gen=" + std::to_string(key.font_generation);
}

struct TextLineLayoutCacheKey {
    std::string text;
    std::string font_descriptor;
    float kern = 0.0f;
    bool explicit_kern = false;
    float wrap_width = 0.0f;
    uint64_t font_generation = 0;

    bool operator==(const TextLineLayoutCacheKey& other) const {
        return text == other.text &&
               font_descriptor == other.font_descriptor &&
               kern == other.kern &&
               explicit_kern == other.explicit_kern &&
               wrap_width == other.wrap_width &&
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
        mix(std::hash<bool>{}(key.explicit_kern));
        mix(std::hash<float>{}(key.wrap_width));
        mix(std::hash<uint64_t>{}(key.font_generation));
        return seed;
    }
};

struct CachedTextLineLayoutFragment {
    CTLineRef line = nullptr;
    CGFloat ascent = 0.0;
    CGFloat descent = 0.0;
    CGFloat leading = 0.0;
    CGFloat width = 0.0;
};

struct CachedTextLineLayout {
    CTLineRef line = nullptr;
    CGFloat ascent = 0.0;
    CGFloat descent = 0.0;
    CGFloat leading = 0.0;
    CGFloat width = 0.0;
    std::vector<CachedTextLineLayoutFragment> wrapped_lines;
    std::list<TextLineLayoutCacheKey>::iterator lru_it{};
};

void release_cached_text_line_layout(CachedTextLineLayout& entry) {
    if (entry.line) {
        CFRelease(entry.line);
        entry.line = nullptr;
    }

    for (auto& fragment : entry.wrapped_lines) {
        if (!fragment.line) {
            continue;
        }
        CFRelease(fragment.line);
        fragment.line = nullptr;
    }
    entry.wrapped_lines.clear();
}

struct TextLineLayoutCacheState {
    std::unordered_map<TextLineLayoutCacheKey, CachedTextLineLayout, TextLineLayoutCacheKeyHash> entries;
    std::list<TextLineLayoutCacheKey> lru_order;
    size_t max_entries = kTextLineLayoutCacheDefaultMaxEntries;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t creations = 0;

    ~TextLineLayoutCacheState() {
        for (auto& [key, entry] : entries) {
            release_cached_text_line_layout(entry);
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
        release_cached_text_line_layout(entry);
    }
    cache.entries.clear();
    cache.lru_order.clear();
}

void text_line_layout_cache_touch_locked(CachedTextLineLayout& entry) {
    auto& cache = text_line_layout_cache_state();
    if (entry.lru_it == cache.lru_order.end() || cache.lru_order.empty()) {
        return;
    }

    auto newest_it = cache.lru_order.end();
    --newest_it;
    if (entry.lru_it == newest_it) {
        return;
    }

    cache.lru_order.splice(cache.lru_order.end(), cache.lru_order, entry.lru_it);
}

void text_line_layout_cache_evict_locked() {
    auto& cache = text_line_layout_cache_state();
    while (cache.entries.size() > cache.max_entries && !cache.lru_order.empty()) {
        auto evict_it = cache.lru_order.begin();
        auto entry_it = cache.entries.find(*evict_it);
        cache.lru_order.erase(evict_it);
        if (entry_it == cache.entries.end()) {
            continue;
        }
        release_cached_text_line_layout(entry_it->second);
        cache.entries.erase(entry_it);
    }
}

std::string text_line_layout_cache_key_for_testing(const TextLineLayoutCacheKey& key) {
    return key.text + "|wrap=" + std::to_string(key.wrap_width) +
           "|kern=" + std::to_string(key.kern) +
           (key.explicit_kern ? "|explicit-kern=1" : "") +
           "|gen=" + std::to_string(key.font_generation);
}

struct TextRunRasterCacheKey {
    std::string text;
    std::string font_descriptor;
    float kern = 0.0f;
    bool explicit_kern = false;
    uint64_t font_generation = 0;
    uint8_t color_r = 0;
    uint8_t color_g = 0;
    uint8_t color_b = 0;
    uint8_t color_a = 255;
    bool smooth_text = true;
    float render_scale = 1.0f;
    int raster_width = 0;
    int raster_height = 0;
    int clipped_x = 0;
    int clipped_y = 0;
    int clipped_width = 0;
    int clipped_height = 0;

    bool operator==(const TextRunRasterCacheKey& other) const {
        return text == other.text &&
               font_descriptor == other.font_descriptor &&
               kern == other.kern &&
               explicit_kern == other.explicit_kern &&
               font_generation == other.font_generation &&
               color_r == other.color_r &&
               color_g == other.color_g &&
               color_b == other.color_b &&
               color_a == other.color_a &&
               smooth_text == other.smooth_text &&
               render_scale == other.render_scale &&
               raster_width == other.raster_width &&
               raster_height == other.raster_height &&
               clipped_x == other.clipped_x &&
               clipped_y == other.clipped_y &&
               clipped_width == other.clipped_width &&
               clipped_height == other.clipped_height;
    }
};

struct TextRunRasterCacheKeyHash {
    size_t operator()(const TextRunRasterCacheKey& key) const {
        size_t seed = std::hash<std::string>{}(key.text);
        auto mix = [&seed](size_t value) {
            seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };
        mix(std::hash<std::string>{}(key.font_descriptor));
        mix(std::hash<float>{}(key.kern));
        mix(std::hash<bool>{}(key.explicit_kern));
        mix(std::hash<uint64_t>{}(key.font_generation));
        mix(std::hash<uint8_t>{}(key.color_r));
        mix(std::hash<uint8_t>{}(key.color_g));
        mix(std::hash<uint8_t>{}(key.color_b));
        mix(std::hash<uint8_t>{}(key.color_a));
        mix(std::hash<bool>{}(key.smooth_text));
        mix(std::hash<float>{}(key.render_scale));
        mix(std::hash<int>{}(key.raster_width));
        mix(std::hash<int>{}(key.raster_height));
        mix(std::hash<int>{}(key.clipped_x));
        mix(std::hash<int>{}(key.clipped_y));
        mix(std::hash<int>{}(key.clipped_width));
        mix(std::hash<int>{}(key.clipped_height));
        return seed;
    }
};

struct CachedTextRunRaster {
    CGImageRef image = nullptr;
    int full_width = 0;
    int full_height = 0;
    int clipped_x = 0;
    int clipped_y = 0;
    int clipped_width = 0;
    int clipped_height = 0;
    std::list<TextRunRasterCacheKey>::iterator lru_it{};
};

struct TextRunRasterCacheState {
    std::unordered_map<TextRunRasterCacheKey, CachedTextRunRaster, TextRunRasterCacheKeyHash> entries;
    std::list<TextRunRasterCacheKey> lru_order;
    size_t max_entries = kTextRunRasterCacheDefaultMaxEntries;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t creations = 0;

    ~TextRunRasterCacheState() {
        for (auto& [key, entry] : entries) {
            if (entry.image) {
                CFRelease(entry.image);
            }
        }
    }
};

struct TextRunRasterScratchState {
    uint8_t* pixels = nullptr;
    CGContextRef context = nullptr;
    CGColorSpaceRef colorspace = nullptr;
    int capacity_width = 0;
    int capacity_height = 0;
    int active_width = 0;
    int active_height = 0;
    size_t bytes = 0;
    uint64_t allocation_count = 0;
    uint64_t context_creation_count = 0;

    ~TextRunRasterScratchState() {
        if (context) {
            CGContextRelease(context);
        }
        if (colorspace) {
            CGColorSpaceRelease(colorspace);
        }
        std::free(pixels);
    }

    void reset() {
        if (context) {
            CGContextRelease(context);
            context = nullptr;
        }
        if (colorspace) {
            CGColorSpaceRelease(colorspace);
            colorspace = nullptr;
        }
        std::free(pixels);
        pixels = nullptr;
        capacity_width = 0;
        capacity_height = 0;
        active_width = 0;
        active_height = 0;
        bytes = 0;
        allocation_count = 0;
        context_creation_count = 0;
    }

    bool ensure(int raster_width, int raster_height, CGColorSpaceRef raster_colorspace) {
        if (raster_width <= 0 || raster_height <= 0 || !raster_colorspace) {
            return false;
        }

        const size_t required_bytes = static_cast<size_t>(raster_width) *
                                      static_cast<size_t>(raster_height) * 4u;
        const bool same_colorspace = colorspace == raster_colorspace ||
            (colorspace && CFEqual(colorspace, raster_colorspace));
        if (same_colorspace && context && pixels &&
            capacity_width >= raster_width && capacity_height >= raster_height &&
            bytes >= required_bytes) {
            active_width = raster_width;
            active_height = raster_height;
            std::memset(pixels, 0, bytes);
            return true;
        }

        if (!pixels || bytes < required_bytes) {
            void* resized_pixels = std::realloc(pixels, required_bytes);
            if (!resized_pixels) {
                return false;
            }
            pixels = static_cast<uint8_t*>(resized_pixels);
            bytes = required_bytes;
            ++allocation_count;
        }

        if (context) {
            CGContextRelease(context);
            context = nullptr;
        }
        if (colorspace) {
            CGColorSpaceRelease(colorspace);
            colorspace = nullptr;
        }

        colorspace = CGColorSpaceRetain(raster_colorspace);
        if (!colorspace) {
            return false;
        }

        context = CGBitmapContextCreate(
            pixels,
            static_cast<size_t>(raster_width),
            static_cast<size_t>(raster_height),
            8,
            static_cast<size_t>(raster_width) * 4,
            colorspace,
            kCGImageAlphaPremultipliedLast
        );
        if (!context) {
            CGColorSpaceRelease(colorspace);
            colorspace = nullptr;
            return false;
        }

        capacity_width = raster_width;
        capacity_height = raster_height;
        active_width = raster_width;
        active_height = raster_height;
        ++context_creation_count;
        std::memset(pixels, 0, bytes);
        return true;
    }
};

struct TextRunRasterLastRequestState {
    int full_width = 0;
    int full_height = 0;
    int clipped_x = 0;
    int clipped_y = 0;
    int clipped_width = 0;
    int clipped_height = 0;
    bool used_clipped_raster = false;

    void reset() {
        full_width = 0;
        full_height = 0;
        clipped_x = 0;
        clipped_y = 0;
        clipped_width = 0;
        clipped_height = 0;
        used_clipped_raster = false;
    }
};

TextRunRasterCacheState& text_run_raster_cache_state() {
    static TextRunRasterCacheState state;
    return state;
}

TextRunRasterScratchState& text_run_raster_scratch_state() {
    thread_local TextRunRasterScratchState state;
    return state;
}

TextRunRasterLastRequestState& text_run_raster_last_request_state() {
    static TextRunRasterLastRequestState state;
    return state;
}

std::mutex& text_run_raster_cache_mutex() {
    static std::mutex mtx;
    return mtx;
}

void invalidate_text_run_raster_cache_locked() {
    auto& cache = text_run_raster_cache_state();
    for (auto& [key, entry] : cache.entries) {
        if (entry.image) {
            CFRelease(entry.image);
        }
    }
    cache.entries.clear();
    cache.lru_order.clear();
}

void text_run_raster_cache_touch_locked(CachedTextRunRaster& entry) {
    auto& cache = text_run_raster_cache_state();
    if (entry.lru_it == cache.lru_order.end() || cache.lru_order.empty()) {
        return;
    }

    auto newest_it = cache.lru_order.end();
    --newest_it;
    if (entry.lru_it == newest_it) {
        return;
    }

    cache.lru_order.splice(cache.lru_order.end(), cache.lru_order, entry.lru_it);
}

void text_run_raster_cache_evict_locked() {
    auto& cache = text_run_raster_cache_state();
    while (cache.entries.size() > cache.max_entries && !cache.lru_order.empty()) {
        auto evict_it = cache.lru_order.begin();
        auto entry_it = cache.entries.find(*evict_it);
        cache.lru_order.erase(evict_it);
        if (entry_it == cache.entries.end()) {
            continue;
        }
        if (entry_it->second.image) {
            CFRelease(entry_it->second.image);
        }
        cache.entries.erase(entry_it);
    }
}

std::string text_run_raster_cache_key_for_testing(const TextRunRasterCacheKey& key) {
    std::string token = key.text + "|scale=" + std::to_string(key.render_scale);
    if (key.explicit_kern) {
        token += "|explicit-kern=1";
    }
    const bool is_clipped =
        key.clipped_x != 0 ||
        key.clipped_y != 0 ||
        key.clipped_width != key.raster_width ||
        key.clipped_height != key.raster_height;
    if (!is_clipped) {
        return token;
    }
    token += "|clip=" + std::to_string(key.clipped_x);
    token += "," + std::to_string(key.clipped_y);
    token += "," + std::to_string(key.clipped_width);
    token += "x" + std::to_string(key.clipped_height);
    return token;
}

struct TextRunRasterSubrect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool empty() const {
        return width <= 0 || height <= 0;
    }
};

TextRunRasterSubrect resolve_text_run_raster_subrect(float draw_x, float draw_y,
                                                     int raster_width, int raster_height,
                                                     int buffer_width, int buffer_height,
                                                     float clip_x, float clip_y,
                                                     float clip_w, float clip_h) {
    TextRunRasterSubrect subrect{0, 0, raster_width, raster_height};
    if (raster_width <= 0 || raster_height <= 0) {
        subrect.width = 0;
        subrect.height = 0;
        return subrect;
    }

    if (!(clip_x >= 0 && clip_w > 0 && clip_h > 0)) {
        return subrect;
    }

    const CGRect draw_bounds = CGRectMake(
        static_cast<CGFloat>(draw_x),
        static_cast<CGFloat>(draw_y),
        static_cast<CGFloat>(raster_width),
        static_cast<CGFloat>(raster_height)
    );
    CGRect clip_bounds = CGRectMake(
        static_cast<CGFloat>(clip_x),
        static_cast<CGFloat>(clip_y),
        static_cast<CGFloat>(clip_w),
        static_cast<CGFloat>(clip_h)
    );
    clip_bounds = CGRectIntersection(
        clip_bounds,
        CGRectMake(0.0, 0.0, static_cast<CGFloat>(buffer_width), static_cast<CGFloat>(buffer_height))
    );
    const CGRect clipped_bounds = CGRectIntersection(draw_bounds, clip_bounds);
    if (CGRectIsNull(clipped_bounds) ||
        CGRectGetWidth(clipped_bounds) <= 0.0 ||
        CGRectGetHeight(clipped_bounds) <= 0.0) {
        subrect.width = 0;
        subrect.height = 0;
        return subrect;
    }

    const int local_x0 = std::max(
        0,
        static_cast<int>(std::floor(CGRectGetMinX(clipped_bounds) - draw_x))
    );
    const int local_y0 = std::max(
        0,
        static_cast<int>(std::floor(CGRectGetMinY(clipped_bounds) - draw_y))
    );
    const int local_x1 = std::min(
        raster_width,
        static_cast<int>(std::ceil(CGRectGetMaxX(clipped_bounds) - draw_x))
    );
    const int local_y1 = std::min(
        raster_height,
        static_cast<int>(std::ceil(CGRectGetMaxY(clipped_bounds) - draw_y))
    );

    subrect.x = local_x0;
    subrect.y = local_y0;
    subrect.width = std::max(0, local_x1 - local_x0);
    subrect.height = std::max(0, local_y1 - local_y0);
    return subrect;
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

std::string cf_description_to_std_string(CFTypeRef value) {
    if (!value) {
        return {};
    }

    CFStringRef description = CFCopyDescription(value);
    if (!description) {
        return {};
    }

    std::string out = cf_string_to_std_string(description);
    CFRelease(description);
    return out;
}

std::string font_layout_descriptor_token(CTFontRef font) {
    if (!font) return {};

    std::string descriptor_attrs_token;
    if (CTFontDescriptorRef descriptor = CTFontCopyFontDescriptor(font)) {
        if (CFDictionaryRef attrs = CTFontDescriptorCopyAttributes(descriptor)) {
            descriptor_attrs_token = cf_description_to_std_string(attrs);
            CFRelease(attrs);
        }
        CFRelease(descriptor);
    }

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
        if (!descriptor_attrs_token.empty()) {
            token += "|attrs=" + descriptor_attrs_token;
        }
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
    if (!descriptor_attrs_token.empty()) {
        token += "|attrs=" + descriptor_attrs_token;
    }
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

CFAttributedStringRef create_context_colored_attributed_text(const std::string& text,
                                                             CTFontRef font,
                                                             CGFloat effective_kern,
                                                             bool include_kern) {
    CFStringRef cf_text = CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(text.data()),
        static_cast<CFIndex>(text.size()),
        kCFStringEncodingUTF8,
        false
    );
    if (!cf_text) {
        return nullptr;
    }

    CFDictionaryRef attrs = nullptr;
    CFNumberRef kern_value = nullptr;
    if (include_kern) {
        kern_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &effective_kern);
        if (kern_value) {
            const void* keys[] = {kCTFontAttributeName, kCTKernAttributeName,
                                  kCTForegroundColorFromContextAttributeName};
            const void* vals[] = {font, kern_value, kCFBooleanTrue};
            attrs = CFDictionaryCreate(
                kCFAllocatorDefault, keys, vals, 3,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
            );
        }
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
        if (kern_value) {
            CFRelease(kern_value);
        }
        return nullptr;
    }

    CFAttributedStringRef attr_str = CFAttributedStringCreate(kCFAllocatorDefault, cf_text, attrs);
    CFRelease(attrs);
    CFRelease(cf_text);
    if (kern_value) {
        CFRelease(kern_value);
    }
    return attr_str;
}

std::vector<CGFloat> wrapped_line_widths_for_layout_request(const std::string& text,
                                                            CTFontRef font,
                                                            CGFloat effective_kern,
                                                            bool include_kern,
                                                            float wrap_width) {
    std::vector<CGFloat> widths;
    if (text.empty() || !font || wrap_width <= 0.0f) {
        return widths;
    }

    const std::string font_descriptor = font_layout_descriptor_token(font);
    const uint64_t font_generation = font_registry_generation().load(std::memory_order_relaxed);
    TextLineLayoutCacheKey cache_key{
        text,
        font_descriptor,
        static_cast<float>(effective_kern),
        include_kern,
        wrap_width,
        font_generation
    };
    {
        std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
        auto& cache = text_line_layout_cache_state();
        auto it = cache.entries.find(cache_key);
        if (it != cache.entries.end() && !it->second.wrapped_lines.empty()) {
            text_line_layout_cache_touch_locked(it->second);
            widths.reserve(it->second.wrapped_lines.size());
            for (const auto& fragment : it->second.wrapped_lines) {
                widths.push_back(fragment.width);
            }
            ++cache.hits;
            return widths;
        }
        ++cache.misses;
    }

    CFAttributedStringRef attr_str = create_context_colored_attributed_text(
        text, font, effective_kern, include_kern
    );
    if (!attr_str) {
        return widths;
    }

    CTTypesetterRef typesetter = CTTypesetterCreateWithAttributedString(attr_str);
    if (!typesetter) {
        CFRelease(attr_str);
        return widths;
    }

    std::vector<CachedTextLineLayoutFragment> wrapped_lines;
    CFIndex text_length = CFAttributedStringGetLength(attr_str);
    CFIndex start = 0;
    while (start < text_length) {
        CFIndex count = CTTypesetterSuggestLineBreak(typesetter, start, static_cast<double>(wrap_width));
        if (count <= 0) {
            count = 1;
        }

        CTLineRef line = CTTypesetterCreateLine(typesetter, CFRangeMake(start, count));
        if (!line) {
            start += count;
            continue;
        }

        CachedTextLineLayoutFragment fragment;
        fragment.line = line;
        fragment.width = static_cast<CGFloat>(CTLineGetTypographicBounds(
            line, &fragment.ascent, &fragment.descent, &fragment.leading
        ));
        widths.push_back(fragment.width);
        wrapped_lines.push_back(fragment);
        start += count;
    }

    CFRelease(typesetter);
    CFRelease(attr_str);

    if (wrapped_lines.empty()) {
        return widths;
    }

    std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
    auto& cache = text_line_layout_cache_state();
    auto it = cache.entries.find(cache_key);
    if (it != cache.entries.end() && !it->second.wrapped_lines.empty()) {
        text_line_layout_cache_touch_locked(it->second);
        widths.clear();
        widths.reserve(it->second.wrapped_lines.size());
        for (const auto& fragment : it->second.wrapped_lines) {
            widths.push_back(fragment.width);
        }
        for (auto& fragment : wrapped_lines) {
            if (fragment.line) {
                CFRelease(fragment.line);
            }
        }
        return widths;
    }

    cache.lru_order.push_back(cache_key);
    auto lru_it = cache.lru_order.end();
    --lru_it;
    CachedTextLineLayout entry;
    entry.wrapped_lines = std::move(wrapped_lines);
    entry.lru_it = lru_it;
    auto [inserted_it, inserted] = cache.entries.emplace(cache_key, std::move(entry));
    if (!inserted) {
        cache.lru_order.pop_back();
        release_cached_text_line_layout(entry);
        widths.clear();
        widths.reserve(inserted_it->second.wrapped_lines.size());
        for (const auto& fragment : inserted_it->second.wrapped_lines) {
            widths.push_back(fragment.width);
        }
        text_line_layout_cache_touch_locked(inserted_it->second);
        return widths;
    }

    ++cache.creations;
    text_line_layout_cache_evict_locked();
    return widths;
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
    {
        std::lock_guard<std::mutex> cache_lock(text_run_raster_cache_mutex());
        invalidate_text_run_raster_cache_locked();
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
    {
        std::lock_guard<std::mutex> cache_lock(text_run_raster_cache_mutex());
        invalidate_text_run_raster_cache_locked();
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
void TextRenderer::render_single_line(const std::string& text, float x, float y,
                                       float font_size, const Color& color,
                                       uint8_t* buffer, int buffer_width, int buffer_height,
                                       CTFontRef font, CGColorRef cg_color, CGColorSpaceRef colorspace,
                                       float letter_spacing, int text_rendering, int font_kerning,
                                       float word_spacing, float clip_x, float clip_y,
                                       float clip_w, float clip_h, float effective_render_scale) {
    if (text.empty()) return;

    // font-kerning: none (2) disables kerning by setting kern to 0
    bool disable_kerning = (font_kerning == 2);
    CGFloat effective_kern = disable_kerning ? 0.0 : static_cast<CGFloat>(letter_spacing);
    const bool include_kern_attribute = letter_spacing != 0 || disable_kerning;

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
    CGFloat descent = 0.0;
    CGFloat leading = 0.0;
    CGFloat line_width = 0.0f;
    const std::string font_descriptor = font_layout_descriptor_token(font);
    const uint64_t font_generation = font_registry_generation().load(std::memory_order_relaxed);
    TextLineLayoutCacheKey cache_key{
        text,
        font_descriptor,
        static_cast<float>(effective_kern),
        include_kern_attribute,
        0.0f,
        font_generation
    };
    {
        std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
        auto& cache = text_line_layout_cache_state();
        auto it = cache.entries.find(cache_key);
        if (it != cache.entries.end()) {
            line = static_cast<CTLineRef>(CFRetain(it->second.line));
            ascent = it->second.ascent;
            descent = it->second.descent;
            leading = it->second.leading;
            line_width = it->second.width;
            text_line_layout_cache_touch_locked(it->second);
            ++cache.hits;
        } else {
            ++cache.misses;
        }
    }

    if (!line) {
        CFAttributedStringRef attr_str = create_context_colored_attributed_text(
            text, font, effective_kern, include_kern_attribute
        );
        if (!attr_str) return;

        line = CTLineCreateWithAttributedString(attr_str);
        if (line) {
            line_width = static_cast<CGFloat>(CTLineGetTypographicBounds(line, &ascent, &descent, &leading));
            std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
            auto& cache = text_line_layout_cache_state();
            auto existing_it = cache.entries.find(cache_key);
            if (existing_it == cache.entries.end()) {
                cache.lru_order.push_back(cache_key);
                auto lru_it = cache.lru_order.end();
                --lru_it;
                CachedTextLineLayout entry;
                entry.line = static_cast<CTLineRef>(CFRetain(line));
                entry.ascent = ascent;
                entry.descent = descent;
                entry.leading = leading;
                entry.width = line_width;
                entry.lru_it = lru_it;
                auto [inserted_it, inserted] = cache.entries.emplace(cache_key, std::move(entry));
                if (inserted) {
                    ++cache.creations;
                    text_line_layout_cache_evict_locked();
                } else {
                    cache.lru_order.pop_back();
                    release_cached_text_line_layout(entry);
                    text_line_layout_cache_touch_locked(inserted_it->second);
                }
            } else {
                text_line_layout_cache_touch_locked(existing_it->second);
            }
        }

        CFRelease(attr_str);
        if (!line) return;
    }

    if (line_width <= 0.0f || font_size <= 0.0f || buffer_width <= 0 || buffer_height <= 0) {
        CGContextRef fallback_ctx = CGBitmapContextCreate(
            buffer, static_cast<size_t>(buffer_width), static_cast<size_t>(buffer_height),
            8, static_cast<size_t>(buffer_width) * 4, colorspace,
            kCGImageAlphaPremultipliedLast
        );
        if (!fallback_ctx) {
            CFRelease(line);
            return;
        }

        if (clip_x >= 0 && clip_w > 0 && clip_h > 0) {
            float cg_clip_y = static_cast<float>(buffer_height) - (clip_y + clip_h);
            CGContextClipToRect(fallback_ctx, CGRectMake(clip_x, cg_clip_y, clip_w, clip_h));
        }

        bool smooth = (text_rendering != 1);
        CGContextSetShouldAntialias(fallback_ctx, smooth);
        CGContextSetAllowsAntialiasing(fallback_ctx, smooth);
        CGContextSetShouldSmoothFonts(fallback_ctx, smooth);
        CGContextSetAllowsFontSmoothing(fallback_ctx, smooth);
        CGContextSetTextDrawingMode(fallback_ctx, kCGTextFill);
        CGContextSetTextMatrix(fallback_ctx, CGAffineTransformIdentity);
        CGContextSetFillColorWithColor(fallback_ctx, cg_color);

        CGFloat fallback_ascent = 0.0;
        CTLineGetTypographicBounds(line, &fallback_ascent, nullptr, nullptr);
        CGFloat baseline_x = static_cast<CGFloat>(x);
        CGFloat baseline_y = static_cast<CGFloat>(buffer_height) - (static_cast<CGFloat>(y) + fallback_ascent);
        CGContextSetTextPosition(fallback_ctx, baseline_x, baseline_y);
        CTLineDraw(line, fallback_ctx);
        CGContextRelease(fallback_ctx);
        CFRelease(line);
        return;
    }

    const int raster_width = std::max(1, static_cast<int>(std::ceil(line_width)));
    const int raster_height = std::max(1, static_cast<int>(std::ceil(ascent + descent + leading)));
    const auto raster_subrect = resolve_text_run_raster_subrect(
        x, y, raster_width, raster_height,
        buffer_width, buffer_height,
        clip_x, clip_y, clip_w, clip_h
    );
    auto& last_request_state = text_run_raster_last_request_state();
    last_request_state.reset();
    last_request_state.full_width = raster_width;
    last_request_state.full_height = raster_height;
    last_request_state.clipped_x = raster_subrect.x;
    last_request_state.clipped_y = raster_subrect.y;
    last_request_state.clipped_width = raster_subrect.width;
    last_request_state.clipped_height = raster_subrect.height;
    last_request_state.used_clipped_raster =
        raster_subrect.x != 0 ||
        raster_subrect.y != 0 ||
        raster_subrect.width != raster_width ||
        raster_subrect.height != raster_height;

    TextRunRasterCacheKey full_run_cache_key{
        text,
        font_descriptor,
        static_cast<float>(effective_kern),
        include_kern_attribute,
        font_generation,
        color.r,
        color.g,
        color.b,
        color.a,
        text_rendering != 1,
        effective_render_scale,
        raster_width,
        raster_height,
        0,
        0,
        raster_width,
        raster_height
    };
    TextRunRasterCacheKey run_cache_key = full_run_cache_key;
    if (last_request_state.used_clipped_raster) {
        run_cache_key.clipped_x = raster_subrect.x;
        run_cache_key.clipped_y = raster_subrect.y;
        run_cache_key.clipped_width = raster_subrect.width;
        run_cache_key.clipped_height = raster_subrect.height;
    }

    CGImageRef cached_raster = nullptr;
    {
        std::lock_guard<std::mutex> cache_lock(text_run_raster_cache_mutex());
        auto& cache = text_run_raster_cache_state();
        auto it = cache.entries.find(run_cache_key);
        if (it == cache.entries.end() && last_request_state.used_clipped_raster) {
            it = cache.entries.find(full_run_cache_key);
        }
        if (it != cache.entries.end() && it->second.image) {
            text_run_raster_cache_touch_locked(it->second);
            cached_raster = reinterpret_cast<CGImageRef>(const_cast<void*>(CFRetain(it->second.image)));
            last_request_state.full_width = it->second.full_width;
            last_request_state.full_height = it->second.full_height;
            last_request_state.clipped_x = it->second.clipped_x;
            last_request_state.clipped_y = it->second.clipped_y;
            last_request_state.clipped_width = it->second.clipped_width;
            last_request_state.clipped_height = it->second.clipped_height;
            last_request_state.used_clipped_raster =
                it->second.clipped_x != 0 ||
                it->second.clipped_y != 0 ||
                it->second.clipped_width != it->second.full_width ||
                it->second.clipped_height != it->second.full_height;
            ++cache.hits;
        } else {
            ++cache.misses;
        }
    }

    if (!cached_raster) {
        if (raster_width <= 0 || raster_height <= 0) {
            CGContextRef fallback_ctx = CGBitmapContextCreate(
                buffer, static_cast<size_t>(buffer_width), static_cast<size_t>(buffer_height),
                8, static_cast<size_t>(buffer_width) * 4, colorspace,
                kCGImageAlphaPremultipliedLast
            );
            if (!fallback_ctx) {
                CFRelease(line);
                return;
            }
            if (clip_x >= 0 && clip_w > 0 && clip_h > 0) {
                float cg_clip_y = static_cast<float>(buffer_height) - (clip_y + clip_h);
                CGContextClipToRect(fallback_ctx, CGRectMake(clip_x, cg_clip_y, clip_w, clip_h));
            }
            bool smooth = (text_rendering != 1);
            CGContextSetShouldAntialias(fallback_ctx, smooth);
            CGContextSetAllowsAntialiasing(fallback_ctx, smooth);
            CGContextSetShouldSmoothFonts(fallback_ctx, smooth);
            CGContextSetAllowsFontSmoothing(fallback_ctx, smooth);
            CGContextSetTextDrawingMode(fallback_ctx, kCGTextFill);
            CGContextSetTextMatrix(fallback_ctx, CGAffineTransformIdentity);
            CGContextSetFillColorWithColor(fallback_ctx, cg_color);

            CGFloat baseline_x = static_cast<CGFloat>(x);
            CGFloat baseline_y = static_cast<CGFloat>(buffer_height) - (static_cast<CGFloat>(y) + ascent);
            CGContextSetTextPosition(fallback_ctx, baseline_x, baseline_y);
            CTLineDraw(line, fallback_ctx);
            CGContextRelease(fallback_ctx);
            CFRelease(line);
            return;
        }
        if (raster_subrect.empty()) {
            CFRelease(line);
            return;
        }

        auto& scratch = text_run_raster_scratch_state();
        if (!scratch.ensure(raster_subrect.width, raster_subrect.height, colorspace)) {
            CGContextRef fallback_ctx = CGBitmapContextCreate(
                buffer, static_cast<size_t>(buffer_width), static_cast<size_t>(buffer_height),
                8, static_cast<size_t>(buffer_width) * 4, colorspace,
                kCGImageAlphaPremultipliedLast
            );
            if (!fallback_ctx) {
                CFRelease(line);
                return;
            }
            if (clip_x >= 0 && clip_w > 0 && clip_h > 0) {
                float cg_clip_y = static_cast<float>(buffer_height) - (clip_y + clip_h);
                CGContextClipToRect(fallback_ctx, CGRectMake(clip_x, cg_clip_y, clip_w, clip_h));
            }
            bool smooth = (text_rendering != 1);
            CGContextSetShouldAntialias(fallback_ctx, smooth);
            CGContextSetAllowsAntialiasing(fallback_ctx, smooth);
            CGContextSetShouldSmoothFonts(fallback_ctx, smooth);
            CGContextSetAllowsFontSmoothing(fallback_ctx, smooth);
            CGContextSetTextDrawingMode(fallback_ctx, kCGTextFill);
            CGContextSetTextMatrix(fallback_ctx, CGAffineTransformIdentity);
            CGContextSetFillColorWithColor(fallback_ctx, cg_color);
            CGFloat baseline_x = static_cast<CGFloat>(x);
            CGFloat baseline_y = static_cast<CGFloat>(buffer_height) - (static_cast<CGFloat>(y) + ascent);
            CGContextSetTextPosition(fallback_ctx, baseline_x, baseline_y);
            CTLineDraw(line, fallback_ctx);
            CGContextRelease(fallback_ctx);
            CFRelease(line);
            return;
        }

        CGContextRef raster_ctx = scratch.context;
        bool smooth = (text_rendering != 1);
        CGContextSaveGState(raster_ctx);
        CGContextClipToRect(
            raster_ctx,
            CGRectMake(
                0.0f,
                static_cast<CGFloat>(scratch.capacity_height - scratch.active_height),
                static_cast<CGFloat>(scratch.active_width),
                static_cast<CGFloat>(scratch.active_height)
            )
        );
        CGContextSetShouldAntialias(raster_ctx, smooth);
        CGContextSetAllowsAntialiasing(raster_ctx, smooth);
        CGContextSetShouldSmoothFonts(raster_ctx, smooth);
        CGContextSetAllowsFontSmoothing(raster_ctx, smooth);
        CGContextSetTextDrawingMode(raster_ctx, kCGTextFill);
        CGContextSetTextMatrix(raster_ctx, CGAffineTransformIdentity);
        CGContextSetFillColorWithColor(raster_ctx, cg_color);
        CGContextSetTextPosition(
            raster_ctx,
            -static_cast<CGFloat>(raster_subrect.x),
            static_cast<CGFloat>(raster_subrect.y + scratch.capacity_height) - ascent
        );
        CTLineDraw(line, raster_ctx);
        CGContextRestoreGState(raster_ctx);
        CGContextFlush(raster_ctx);

        CGImageRef raster_image = CGBitmapContextCreateImage(raster_ctx);
        if (raster_image &&
            (scratch.capacity_width != scratch.active_width ||
             scratch.capacity_height != scratch.active_height)) {
            CGImageRef cropped_image = CGImageCreateWithImageInRect(
                raster_image,
                CGRectMake(
                    0.0f,
                    0.0f,
                    static_cast<CGFloat>(scratch.active_width),
                    static_cast<CGFloat>(scratch.active_height)
                )
            );
            CFRelease(raster_image);
            raster_image = cropped_image;
        }
        if (!raster_image) {
            CGContextRef fallback_ctx = CGBitmapContextCreate(
                buffer, static_cast<size_t>(buffer_width), static_cast<size_t>(buffer_height),
                8, static_cast<size_t>(buffer_width) * 4, colorspace,
                kCGImageAlphaPremultipliedLast
            );
            if (!fallback_ctx) {
                CFRelease(line);
                return;
            }
            if (clip_x >= 0 && clip_w > 0 && clip_h > 0) {
                float cg_clip_y = static_cast<float>(buffer_height) - (clip_y + clip_h);
                CGContextClipToRect(fallback_ctx, CGRectMake(clip_x, cg_clip_y, clip_w, clip_h));
            }
            bool smooth = (text_rendering != 1);
            CGContextSetShouldAntialias(fallback_ctx, smooth);
            CGContextSetAllowsAntialiasing(fallback_ctx, smooth);
            CGContextSetShouldSmoothFonts(fallback_ctx, smooth);
            CGContextSetAllowsFontSmoothing(fallback_ctx, smooth);
            CGContextSetTextDrawingMode(fallback_ctx, kCGTextFill);
            CGContextSetTextMatrix(fallback_ctx, CGAffineTransformIdentity);
            CGContextSetFillColorWithColor(fallback_ctx, cg_color);
            CGFloat baseline_x = static_cast<CGFloat>(x);
            CGFloat baseline_y = static_cast<CGFloat>(buffer_height) - (static_cast<CGFloat>(y) + ascent);
            CGContextSetTextPosition(fallback_ctx, baseline_x, baseline_y);
            CTLineDraw(line, fallback_ctx);
            CGContextRelease(fallback_ctx);
            CFRelease(line);
            return;
        }

        bool inserted = false;
        {
            std::lock_guard<std::mutex> cache_lock(text_run_raster_cache_mutex());
            auto& cache = text_run_raster_cache_state();
            auto it = cache.entries.find(run_cache_key);
            if (it == cache.entries.end()) {
                cache.lru_order.push_back(run_cache_key);
                auto lru_it = cache.lru_order.end();
                --lru_it;
                CachedTextRunRaster cached_entry;
                cached_entry.image = raster_image;
                cached_entry.full_width = raster_width;
                cached_entry.full_height = raster_height;
                cached_entry.clipped_x = raster_subrect.x;
                cached_entry.clipped_y = raster_subrect.y;
                cached_entry.clipped_width = raster_subrect.width;
                cached_entry.clipped_height = raster_subrect.height;
                cached_entry.lru_it = lru_it;
                auto emplace_result = cache.entries.emplace(
                    run_cache_key,
                    std::move(cached_entry)
                );
                inserted = emplace_result.second;
                if (inserted) {
                    ++cache.creations;
                    text_run_raster_cache_evict_locked();
                    cached_raster = reinterpret_cast<CGImageRef>(const_cast<void*>(CFRetain(raster_image)));
                } else {
                    cache.lru_order.pop_back();
                    auto existing_it = cache.entries.find(run_cache_key);
                    if (existing_it != cache.entries.end() && existing_it->second.image) {
                        text_run_raster_cache_touch_locked(existing_it->second);
                        cached_raster = reinterpret_cast<CGImageRef>(const_cast<void*>(CFRetain(existing_it->second.image)));
                        last_request_state.full_width = existing_it->second.full_width;
                        last_request_state.full_height = existing_it->second.full_height;
                        last_request_state.clipped_x = existing_it->second.clipped_x;
                        last_request_state.clipped_y = existing_it->second.clipped_y;
                        last_request_state.clipped_width = existing_it->second.clipped_width;
                        last_request_state.clipped_height = existing_it->second.clipped_height;
                        last_request_state.used_clipped_raster =
                            existing_it->second.clipped_x != 0 ||
                            existing_it->second.clipped_y != 0 ||
                            existing_it->second.clipped_width != existing_it->second.full_width ||
                            existing_it->second.clipped_height != existing_it->second.full_height;
                    }
                }
            } else if (it->second.image) {
                text_run_raster_cache_touch_locked(it->second);
                cached_raster = reinterpret_cast<CGImageRef>(const_cast<void*>(CFRetain(it->second.image)));
                last_request_state.full_width = it->second.full_width;
                last_request_state.full_height = it->second.full_height;
                last_request_state.clipped_x = it->second.clipped_x;
                last_request_state.clipped_y = it->second.clipped_y;
                last_request_state.clipped_width = it->second.clipped_width;
                last_request_state.clipped_height = it->second.clipped_height;
                last_request_state.used_clipped_raster =
                    it->second.clipped_x != 0 ||
                    it->second.clipped_y != 0 ||
                    it->second.clipped_width != it->second.full_width ||
                    it->second.clipped_height != it->second.full_height;
            }
        }
        if (!inserted) {
            CFRelease(raster_image);
        }
    }

    if (!cached_raster) {
        CGContextRef fallback_ctx = CGBitmapContextCreate(
            buffer, static_cast<size_t>(buffer_width), static_cast<size_t>(buffer_height),
            8, static_cast<size_t>(buffer_width) * 4, colorspace,
            kCGImageAlphaPremultipliedLast
        );
        if (!fallback_ctx) {
            CFRelease(line);
            return;
        }
        if (clip_x >= 0 && clip_w > 0 && clip_h > 0) {
            float cg_clip_y = static_cast<float>(buffer_height) - (clip_y + clip_h);
            CGContextClipToRect(fallback_ctx, CGRectMake(clip_x, cg_clip_y, clip_w, clip_h));
        }
        bool smooth = (text_rendering != 1);
        CGContextSetShouldAntialias(fallback_ctx, smooth);
        CGContextSetAllowsAntialiasing(fallback_ctx, smooth);
        CGContextSetShouldSmoothFonts(fallback_ctx, smooth);
        CGContextSetAllowsFontSmoothing(fallback_ctx, smooth);
        CGContextSetTextDrawingMode(fallback_ctx, kCGTextFill);
        CGContextSetTextMatrix(fallback_ctx, CGAffineTransformIdentity);
        CGContextSetFillColorWithColor(fallback_ctx, cg_color);
        CGFloat baseline_x = static_cast<CGFloat>(x);
        CGFloat baseline_y = static_cast<CGFloat>(buffer_height) - (static_cast<CGFloat>(y) + ascent);
        CGContextSetTextPosition(fallback_ctx, baseline_x, baseline_y);
        CTLineDraw(line, fallback_ctx);
        CGContextRelease(fallback_ctx);
        CFRelease(line);
        return;
    }

    CGContextRef ctx = CGBitmapContextCreate(
        buffer, static_cast<size_t>(buffer_width), static_cast<size_t>(buffer_height),
        8, static_cast<size_t>(buffer_width) * 4, colorspace,
        kCGImageAlphaPremultipliedLast
    );
    if (!ctx) {
        if (cached_raster) {
            CFRelease(cached_raster);
        }
        CFRelease(line);
        return;
    }

    if (clip_x >= 0 && clip_w > 0 && clip_h > 0) {
        float cg_clip_y = static_cast<float>(buffer_height) - (clip_y + clip_h);
        CGContextClipToRect(ctx, CGRectMake(clip_x, cg_clip_y, clip_w, clip_h));
    }

    CGContextSetShouldAntialias(ctx, text_rendering != 1);
    CGContextSetAllowsAntialiasing(ctx, text_rendering != 1);
    CGContextSetShouldSmoothFonts(ctx, text_rendering != 1);
    CGContextSetAllowsFontSmoothing(ctx, text_rendering != 1);
    const auto& last_request = text_run_raster_last_request_state();
    const float draw_x = cached_raster && last_request.used_clipped_raster
        ? x + static_cast<float>(last_request.clipped_x)
        : x;
    const float draw_y = cached_raster && last_request.used_clipped_raster
        ? y + static_cast<float>(last_request.clipped_y)
        : y;
    const CGFloat cached_raster_height = static_cast<CGFloat>(CGImageGetHeight(cached_raster));
    CGContextDrawImage(
        ctx,
        CGRectMake(
            draw_x,
            static_cast<float>(buffer_height) - draw_y - cached_raster_height,
            static_cast<CGFloat>(CGImageGetWidth(cached_raster)),
            cached_raster_height
        ),
        cached_raster
    );

    CGContextRelease(ctx);
    CFRelease(cached_raster);
    CFRelease(line);
    return;
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
                                float clip_x, float clip_y, float clip_w, float clip_h,
                                float effective_render_scale) {
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
                                   clip_x, clip_y, clip_w, clip_h,
                                   effective_render_scale);
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
                           clip_x, clip_y, clip_w, clip_h,
                           effective_render_scale);
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
    const uint64_t font_generation = font_registry_generation().load(std::memory_order_relaxed);
    TextWidthCacheKey key{text, font_size, normalize_family(font_family),
                          font_weight, font_italic, letter_spacing, word_spacing,
                          font_generation};
    {
        std::lock_guard<std::mutex> cache_lock(text_width_cache_mutex());
        auto& cache = text_width_cache_state();
        auto it = cache.entries.find(key);
        if (it != cache.entries.end()) {
            ++cache.hits;
            text_width_cache_touch_locked(it->second);
            return it->second.width;
        }
        ++cache.misses;
    }

    float width = measure_text_width_uncached(text, font_size, font_family,
                                              font_weight, font_italic,
                                              letter_spacing, word_spacing);

    std::lock_guard<std::mutex> cache_lock(text_width_cache_mutex());
    auto& cache = text_width_cache_state();
    auto existing_it = cache.entries.find(key);
    if (existing_it != cache.entries.end()) {
        existing_it->second.width = width;
        text_width_cache_touch_locked(existing_it->second);
        return existing_it->second.width;
    }

    cache.lru_order.push_back(key);
    auto lru_it = cache.lru_order.end();
    --lru_it;
    cache.entries.emplace(std::move(key), CachedTextWidth{width, lru_it});
    text_width_cache_evict_locked();
    return width;
}

void reset_text_width_cache_stats_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_width_cache_mutex());
    auto& cache = text_width_cache_state();
    cache.hits = 0;
    cache.misses = 0;
}

void set_text_width_cache_max_entries_for_testing(size_t max_entries) {
    std::lock_guard<std::mutex> cache_lock(text_width_cache_mutex());
    auto& cache = text_width_cache_state();
    cache.max_entries = std::max<size_t>(1, max_entries);
    text_width_cache_evict_locked();
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

std::vector<std::string> text_width_cache_lru_order_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_width_cache_mutex());
    std::vector<std::string> order;
    order.reserve(text_width_cache_state().lru_order.size());
    for (const auto& key : text_width_cache_state().lru_order) {
        order.push_back(text_width_cache_key_for_testing(key));
    }
    return order;
}

void reset_text_line_layout_cache_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
    auto& cache = text_line_layout_cache_state();
    invalidate_text_line_layout_cache_locked();
    cache.max_entries = kTextLineLayoutCacheDefaultMaxEntries;
    cache.hits = 0;
    cache.misses = 0;
    cache.creations = 0;
}

void set_text_line_layout_cache_max_entries_for_testing(size_t max_entries) {
    std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
    auto& cache = text_line_layout_cache_state();
    cache.max_entries = std::max<size_t>(1, max_entries);
    text_line_layout_cache_evict_locked();
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

std::vector<std::string> text_line_layout_cache_lru_order_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_line_layout_cache_mutex());
    std::vector<std::string> order;
    order.reserve(text_line_layout_cache_state().lru_order.size());
    for (const auto& key : text_line_layout_cache_state().lru_order) {
        order.push_back(text_line_layout_cache_key_for_testing(key));
    }
    return order;
}

std::vector<float> text_line_layout_wrapped_widths_for_testing(const std::string& text,
                                                               float wrap_width,
                                                               float font_size,
                                                               const std::string& font_family,
                                                               int font_weight,
                                                               bool font_italic,
                                                               float letter_spacing) {
    if (text.empty() || wrap_width <= 0.0f) {
        return {};
    }
    if (font_size < 1.0f) {
        font_size = 1.0f;
    }

    CTFontRef base_font = create_web_font(font_family, static_cast<CGFloat>(font_size),
                                          font_weight, font_italic);
    if (!base_font) {
        CFStringRef ct_font_name = font_name_for_family(font_family);
        base_font = CTFontCreateWithName(ct_font_name, static_cast<CGFloat>(font_size), nullptr);
    }
    if (!base_font) {
        base_font = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, static_cast<CGFloat>(font_size), nullptr);
    }
    if (!base_font) {
        return {};
    }

    CTFontRef font = base_font;
    bool is_web_font = TextRenderer::has_registered_font(font_family);
    if (!is_web_font) {
        CTFontSymbolicTraits traits = 0;
        if (font_weight >= 600) {
            traits |= kCTFontBoldTrait;
        }
        if (font_italic) {
            traits |= kCTFontItalicTrait;
        }

        if (traits != 0) {
            CTFontRef styled_font = CTFontCreateCopyWithSymbolicTraits(base_font, 0.0, nullptr, traits, traits);
            if (styled_font) {
                CFRelease(base_font);
                font = styled_font;
            }
        }
    }

    std::vector<CGFloat> cg_widths = wrapped_line_widths_for_layout_request(
        text,
        font,
        static_cast<CGFloat>(letter_spacing),
        letter_spacing != 0.0f,
        wrap_width
    );

    std::vector<float> widths;
    widths.reserve(cg_widths.size());
    for (CGFloat width : cg_widths) {
        widths.push_back(static_cast<float>(width));
    }

    CFRelease(font);
    return widths;
}

void reset_text_run_raster_cache_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_run_raster_cache_mutex());
    auto& cache = text_run_raster_cache_state();
    invalidate_text_run_raster_cache_locked();
    text_run_raster_scratch_state().reset();
    text_run_raster_last_request_state().reset();
    cache.max_entries = kTextRunRasterCacheDefaultMaxEntries;
    cache.hits = 0;
    cache.misses = 0;
    cache.creations = 0;
}

void set_text_run_raster_cache_max_entries_for_testing(size_t max_entries) {
    std::lock_guard<std::mutex> cache_lock(text_run_raster_cache_mutex());
    auto& cache = text_run_raster_cache_state();
    cache.max_entries = std::max<size_t>(1, max_entries);
    text_run_raster_cache_evict_locked();
}

size_t text_run_raster_cache_size_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_run_raster_cache_mutex());
    return text_run_raster_cache_state().entries.size();
}

uint64_t text_run_raster_cache_hit_count_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_run_raster_cache_mutex());
    return text_run_raster_cache_state().hits;
}

uint64_t text_run_raster_cache_miss_count_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_run_raster_cache_mutex());
    return text_run_raster_cache_state().misses;
}

uint64_t text_run_raster_cache_creation_count_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_run_raster_cache_mutex());
    return text_run_raster_cache_state().creations;
}

uint64_t text_run_raster_scratch_allocation_count_for_testing() {
    return text_run_raster_scratch_state().allocation_count;
}

uint64_t text_run_raster_scratch_context_creation_count_for_testing() {
    return text_run_raster_scratch_state().context_creation_count;
}

int text_run_raster_last_full_width_for_testing() {
    return text_run_raster_last_request_state().full_width;
}

int text_run_raster_last_full_height_for_testing() {
    return text_run_raster_last_request_state().full_height;
}

int text_run_raster_last_clipped_x_for_testing() {
    return text_run_raster_last_request_state().clipped_x;
}

int text_run_raster_last_clipped_y_for_testing() {
    return text_run_raster_last_request_state().clipped_y;
}

int text_run_raster_last_clipped_width_for_testing() {
    return text_run_raster_last_request_state().clipped_width;
}

int text_run_raster_last_clipped_height_for_testing() {
    return text_run_raster_last_request_state().clipped_height;
}

bool text_run_raster_last_request_used_clipped_raster_for_testing() {
    return text_run_raster_last_request_state().used_clipped_raster;
}

std::vector<std::string> text_run_raster_cache_lru_order_for_testing() {
    std::lock_guard<std::mutex> cache_lock(text_run_raster_cache_mutex());
    std::vector<std::string> order;
    order.reserve(text_run_raster_cache_state().lru_order.size());
    for (const auto& key : text_run_raster_cache_state().lru_order) {
        order.push_back(text_run_raster_cache_key_for_testing(key));
    }
    return order;
}

} // namespace clever::paint
