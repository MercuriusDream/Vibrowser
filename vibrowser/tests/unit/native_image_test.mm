// Native image decoding tests (CGImageSource on macOS)
// Separate .mm file to avoid QuickDraw Rect conflict with clever::paint::Rect

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#include <gtest/gtest.h>
#include <vector>
#include <cstdint>

// Helper: create a minimal PNG image in memory using CoreGraphics
static std::vector<uint8_t> create_test_png(int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(nullptr, w, h, 8, w * 4, cs,
        static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);
    CGContextSetRGBFillColor(ctx, r / 255.0, g / 255.0, b / 255.0, 1.0);
    CGContextFillRect(ctx, CGRectMake(0, 0, w, h));
    CGImageRef image = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);

    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CGImageDestinationRef dest = CGImageDestinationCreateWithData(data, CFSTR("public.png"), 1, nullptr);
    CGImageDestinationAddImage(dest, image, nullptr);
    CGImageDestinationFinalize(dest);
    CFRelease(dest);
    CGImageRelease(image);

    const uint8_t* bytes = CFDataGetBytePtr(data);
    size_t len = static_cast<size_t>(CFDataGetLength(data));
    std::vector<uint8_t> result(bytes, bytes + len);
    CFRelease(data);
    return result;
}

// Helper: create a 2x2 PNG with explicit RGBA pixels in row-major (top-left origin)
static std::vector<uint8_t> create_test_png_2x2_rgba(const uint8_t* rgba_16_bytes) {
    const int w = 2;
    const int h = 2;

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        const_cast<uint8_t*>(rgba_16_bytes), w, h, 8, w * 4, cs,
        static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);
    if (!ctx) return {};

    CGImageRef image = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    if (!image) return {};

    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CGImageDestinationRef dest = CGImageDestinationCreateWithData(data, CFSTR("public.png"), 1, nullptr);
    if (!dest) {
        CGImageRelease(image);
        CFRelease(data);
        return {};
    }
    CGImageDestinationAddImage(dest, image, nullptr);
    CGImageDestinationFinalize(dest);
    CFRelease(dest);
    CGImageRelease(image);

    const uint8_t* bytes = CFDataGetBytePtr(data);
    size_t len = static_cast<size_t>(CFDataGetLength(data));
    std::vector<uint8_t> result(bytes, bytes + len);
    CFRelease(data);
    return result;
}

TEST(NativeImageDecode, CGImageSourceDecodesPNG) {
    // Create a 4x4 red PNG
    auto png_data = create_test_png(4, 4, 255, 0, 0);
    ASSERT_FALSE(png_data.empty());

    // Verify CGImageSource can decode it
    CFDataRef cf_data = CFDataCreate(kCFAllocatorDefault, png_data.data(),
        static_cast<CFIndex>(png_data.size()));
    CGImageSourceRef source = CGImageSourceCreateWithData(cf_data, nullptr);
    ASSERT_NE(source, nullptr);

    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(CGImageGetWidth(image), 4u);
    EXPECT_EQ(CGImageGetHeight(image), 4u);

    CGImageRelease(image);
    CFRelease(source);
    CFRelease(cf_data);
}

TEST(NativeImageDecode, CGImageSourceRejectsInvalidData) {
    uint8_t not_image[] = {0, 1, 2, 3};
    CFDataRef cf_data = CFDataCreate(kCFAllocatorDefault, not_image, 4);
    CGImageSourceRef source = CGImageSourceCreateWithData(cf_data, nullptr);
    if (source) {
        CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
        EXPECT_EQ(image, nullptr);
        if (image) CGImageRelease(image);
        CFRelease(source);
    }
    CFRelease(cf_data);
}

TEST(NativeImageDecode, CGImageSourceSupportsWebP) {
    CFArrayRef types = CGImageSourceCopyTypeIdentifiers();
    ASSERT_NE(types, nullptr);
    bool has_webp = false;
    CFIndex count = CFArrayGetCount(types);
    for (CFIndex i = 0; i < count; ++i) {
        CFStringRef type = static_cast<CFStringRef>(CFArrayGetValueAtIndex(types, i));
        if (CFStringFind(type, CFSTR("webp"), kCFCompareCaseInsensitive).location != kCFNotFound) {
            has_webp = true;
            break;
        }
    }
    CFRelease(types);
    EXPECT_TRUE(has_webp) << "WebP not supported by CGImageSource on this macOS version";
}

TEST(NativeImageDecode, CGImageSourceSupportsHEIC) {
    CFArrayRef types = CGImageSourceCopyTypeIdentifiers();
    ASSERT_NE(types, nullptr);
    bool has_heic = false;
    CFIndex count = CFArrayGetCount(types);
    for (CFIndex i = 0; i < count; ++i) {
        CFStringRef type = static_cast<CFStringRef>(CFArrayGetValueAtIndex(types, i));
        if (CFStringFind(type, CFSTR("heic"), kCFCompareCaseInsensitive).location != kCFNotFound) {
            has_heic = true;
            break;
        }
    }
    CFRelease(types);
    EXPECT_TRUE(has_heic) << "HEIC not supported by CGImageSource on this macOS version";
}

TEST(NativeImageDecode, DecodePNGToRGBAPixels) {
    // Create a 2x2 image with distinct corners so orientation regressions are detectable.
    // Row 0 (top):    red, green
    // Row 1 (bottom): blue, yellow
    const uint8_t src_rgba[] = {
        255, 0,   0,   255,   0,   255, 0,   255,
        0,   0,   255, 255,   255, 255, 0,   255
    };
    auto png_data = create_test_png_2x2_rgba(src_rgba);
    ASSERT_FALSE(png_data.empty());

    CFDataRef cf_data = CFDataCreate(kCFAllocatorDefault, png_data.data(),
        static_cast<CFIndex>(png_data.size()));
    CGImageSourceRef source = CGImageSourceCreateWithData(cf_data, nullptr);
    ASSERT_NE(source, nullptr);
    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    ASSERT_NE(image, nullptr);

    // Render to RGBA buffer (same technique as decode_image_native)
    int w = static_cast<int>(CGImageGetWidth(image));
    int h = static_cast<int>(CGImageGetHeight(image));
    std::vector<uint8_t> pixels(w * h * 4, 0);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(pixels.data(), w, h, 8, w * 4, cs,
        static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);
    CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), image);
    CGContextRelease(ctx);
    CGImageRelease(image);
    CFRelease(source);
    CFRelease(cf_data);

    ASSERT_EQ(w, 2);
    ASSERT_EQ(h, 2);

    auto at = [&](int x, int y, int c) -> uint8_t {
        return pixels[(y * w + x) * 4 + c];
    };

    // Top-left: red
    EXPECT_EQ(at(0, 0, 0), 255);
    EXPECT_EQ(at(0, 0, 1), 0);
    EXPECT_EQ(at(0, 0, 2), 0);
    EXPECT_EQ(at(0, 0, 3), 255);
    // Top-right: green
    EXPECT_EQ(at(1, 0, 0), 0);
    EXPECT_EQ(at(1, 0, 1), 255);
    EXPECT_EQ(at(1, 0, 2), 0);
    EXPECT_EQ(at(1, 0, 3), 255);
    // Bottom-left: blue
    EXPECT_EQ(at(0, 1, 0), 0);
    EXPECT_EQ(at(0, 1, 1), 0);
    EXPECT_EQ(at(0, 1, 2), 255);
    EXPECT_EQ(at(0, 1, 3), 255);
    // Bottom-right: yellow
    EXPECT_EQ(at(1, 1, 0), 255);
    EXPECT_EQ(at(1, 1, 1), 255);
    EXPECT_EQ(at(1, 1, 2), 0);
    EXPECT_EQ(at(1, 1, 3), 255);
}
