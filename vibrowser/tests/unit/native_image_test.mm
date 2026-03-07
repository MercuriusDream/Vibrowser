// Native image decoding tests (CGImageSource on macOS)
// Separate .mm file to avoid QuickDraw Rect conflict with clever::paint::Rect

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#import "../../src/shell/browser_window.h"
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

TEST(RenderViewGeometry, ExtractBufferRectUsesRendererScale) {
    clever::paint::Rect bounds{10.0f, 12.0f, 30.0f, 20.0f};
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    ASSERT_TRUE(render_view_extract_buffer_rect(bounds, 2.0, 200, 200, &x, &y, &w, &h));
    EXPECT_EQ(x, 20);
    EXPECT_EQ(y, 24);
    EXPECT_EQ(w, 60);
    EXPECT_EQ(h, 40);
}

TEST(RenderViewGeometry, ExtractBufferRectClipsOffscreenBounds) {
    clever::paint::Rect bounds{-5.0f, 8.0f, 20.0f, 10.0f};
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    ASSERT_TRUE(render_view_extract_buffer_rect(bounds, 2.0, 100, 100, &x, &y, &w, &h));
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 16);
    EXPECT_EQ(w, 30);
    EXPECT_EQ(h, 20);
}

TEST(RenderViewGeometry, StickyOverlayResyncPreservesClippedLeftOrigin) {
    StickyElementInfo sticky;
    sticky.pixel_x = 0;
    sticky.pixel_width = 40;
    sticky.pixel_height = 20;
    sticky.renderer_dpr = 2.0f;
    sticky.logical_x = 5.0f;
    sticky.logical_width = 30.0f;
    sticky.logical_height = 10.0f;

    render_view_resync_sticky_overlay_geometry(sticky, 1.0);

    EXPECT_FLOAT_EQ(sticky.logical_x, 5.0f);
    EXPECT_FLOAT_EQ(sticky.logical_width, 30.0f);
    EXPECT_FLOAT_EQ(sticky.logical_height, 10.0f);
}

TEST(RenderViewGeometry, StickyOverlayResyncKeepsDocumentOriginForSliceLocalPixels) {
    StickyElementInfo sticky;
    sticky.pixel_x = 24;
    sticky.pixel_width = 40;
    sticky.pixel_height = 20;
    sticky.renderer_dpr = 2.0f;
    sticky.logical_x = 128.0f;
    sticky.logical_width = 30.0f;
    sticky.logical_height = 12.0f;

    render_view_resync_sticky_overlay_geometry(sticky, 1.0);

    EXPECT_FLOAT_EQ(sticky.logical_x, 128.0f);
    EXPECT_FLOAT_EQ(sticky.logical_width, 30.0f);
    EXPECT_FLOAT_EQ(sticky.logical_height, 12.0f);
}

TEST(RenderViewGeometry, StickyOverlayViewportRectTracksDocumentGeometryAcrossScaleChanges) {
    StickyElementInfo sticky;
    sticky.abs_y = 60.0f;
    sticky.height = 10.0f;
    sticky.top_offset = 4.0f;
    sticky.pixel_x = 24;
    sticky.pixel_width = 40;
    sticky.pixel_height = 20;
    sticky.renderer_dpr = 2.0f;
    sticky.logical_x = 128.0f;
    sticky.logical_width = 30.0f;
    sticky.logical_height = 12.0f;

    render_view_resync_sticky_overlay_geometry(sticky, 3.0);

    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_sticky_overlay_viewport_rect(
        sticky, 120.0f, 80.0f, 0.0, 400.0f, &viewportRect));
    EXPECT_FLOAT_EQ(viewportRect.x, 8.0f);
    EXPECT_FLOAT_EQ(viewportRect.y, 4.0f);
    EXPECT_FLOAT_EQ(viewportRect.width, 20.0f);
    EXPECT_FLOAT_EQ(viewportRect.height, 10.0f);

    RenderViewOverlayRect exportRect = render_view_scale_overlay_rect(viewportRect, 3.0);
    EXPECT_FLOAT_EQ(exportRect.x, 24.0f);
    EXPECT_FLOAT_EQ(exportRect.y, 12.0f);
    EXPECT_FLOAT_EQ(exportRect.width, 60.0f);
    EXPECT_FLOAT_EQ(exportRect.height, 30.0f);
}

TEST(RenderViewGeometry, StickyOverlayViewportRectUsesVisibleSliceForLeftClippedSnapshot) {
    StickyElementInfo sticky;
    sticky.abs_y = 60.0f;
    sticky.height = 10.0f;
    sticky.top_offset = 4.0f;
    sticky.pixel_x = 0;
    sticky.pixel_width = 20;
    sticky.pixel_height = 20;
    sticky.renderer_dpr = 2.0f;
    sticky.logical_x = 110.0f;
    sticky.logical_width = 20.0f;
    sticky.logical_height = 10.0f;

    render_view_resync_sticky_overlay_geometry(sticky, 3.0);

    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_sticky_overlay_viewport_rect(
        sticky, 120.0f, 80.0f, 0.0, 400.0f, &viewportRect));
    EXPECT_FLOAT_EQ(viewportRect.x, 0.0f);
    EXPECT_FLOAT_EQ(viewportRect.y, 4.0f);
    EXPECT_FLOAT_EQ(viewportRect.width, 10.0f);
    EXPECT_FLOAT_EQ(viewportRect.height, 10.0f);

    RenderViewOverlayRect exportRect = render_view_scale_overlay_rect(viewportRect, 3.0);
    EXPECT_FLOAT_EQ(exportRect.x, 0.0f);
    EXPECT_FLOAT_EQ(exportRect.y, 12.0f);
    EXPECT_FLOAT_EQ(exportRect.width, 30.0f);
    EXPECT_FLOAT_EQ(exportRect.height, 30.0f);
}

TEST(RenderViewGeometry, ContainerStickyViewportRectSubtractsContainerScrollX) {
    StickyElementInfo sticky;
    sticky.abs_y = 72.0f;
    sticky.height = 10.0f;
    sticky.top_offset = 6.0f;
    sticky.container_bottom = 200.0f;
    sticky.container_scroll_x = 18.0f;
    sticky.container_scroll_y = 40.0f;
    sticky.container_y = 30.0f;
    sticky.is_page_sticky = false;
    sticky.pixel_width = 24;
    sticky.pixel_height = 12;
    sticky.renderer_dpr = 2.0f;
    sticky.logical_x = 150.0f;

    render_view_resync_sticky_overlay_geometry(sticky, 1.0);

    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_sticky_overlay_viewport_rect(
        sticky, 100.0f, 20.0f, 0.0, 400.0f, &viewportRect));
    EXPECT_FLOAT_EQ(viewportRect.x, 32.0f);
    EXPECT_FLOAT_EQ(viewportRect.y, 16.0f);
    EXPECT_FLOAT_EQ(viewportRect.width, 12.0f);
    EXPECT_FLOAT_EQ(viewportRect.height, 6.0f);
}

TEST(RenderViewGeometry, FixedOverlayResyncUsesSnapshotDpr) {
    FixedElementInfo fixed;
    fixed.pixel_width = 120;
    fixed.pixel_height = 48;
    fixed.renderer_dpr = 2.0f;

    render_view_resync_fixed_overlay_geometry(fixed, 1.0);

    EXPECT_FLOAT_EQ(fixed.logical_width, 60.0f);
    EXPECT_FLOAT_EQ(fixed.logical_height, 24.0f);
}

TEST(RenderViewGeometry, FixedOverlayResyncClampsNegativeViewportOrigin) {
    FixedElementInfo fixed;
    fixed.viewport_x = -12.0f;
    fixed.viewport_y = -4.0f;
    fixed.pixel_width = 40;
    fixed.pixel_height = 20;
    fixed.renderer_dpr = 2.0f;

    render_view_resync_fixed_overlay_geometry(fixed, 1.0);

    EXPECT_FLOAT_EQ(fixed.viewport_x, 0.0f);
    EXPECT_FLOAT_EQ(fixed.viewport_y, 0.0f);
    EXPECT_FLOAT_EQ(fixed.logical_width, 20.0f);
    EXPECT_FLOAT_EQ(fixed.logical_height, 10.0f);
}

TEST(RenderViewGeometry, FixedOverlayResyncTopLeftClipUsesSnapshotDpr) {
    FixedElementInfo fixed;
    fixed.viewport_x = -8.0f;
    fixed.viewport_y = -4.0f;
    fixed.pixel_width = 32;
    fixed.pixel_height = 20;
    fixed.renderer_dpr = 2.0f;
    fixed.logical_width = 24.0f;
    fixed.logical_height = 14.0f;

    render_view_resync_fixed_overlay_geometry(fixed, 1.0);

    EXPECT_FLOAT_EQ(fixed.viewport_x, 0.0f);
    EXPECT_FLOAT_EQ(fixed.viewport_y, 0.0f);
    EXPECT_FLOAT_EQ(fixed.logical_width, 16.0f);
    EXPECT_FLOAT_EQ(fixed.logical_height, 10.0f);
}

TEST(RenderViewGeometry, FixedOverlayViewportRectUsesCurrentExportScale) {
    FixedElementInfo fixed;
    fixed.viewport_x = 5.0f;
    fixed.viewport_y = 7.0f;
    fixed.pixel_width = 40;
    fixed.pixel_height = 20;
    fixed.renderer_dpr = 2.0f;

    render_view_resync_fixed_overlay_geometry(fixed, 3.0);

    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_fixed_overlay_viewport_rect(fixed, &viewportRect));
    EXPECT_FLOAT_EQ(viewportRect.x, 5.0f);
    EXPECT_FLOAT_EQ(viewportRect.y, 7.0f);
    EXPECT_FLOAT_EQ(viewportRect.width, 20.0f);
    EXPECT_FLOAT_EQ(viewportRect.height, 10.0f);

    RenderViewOverlayRect exportRect = render_view_scale_overlay_rect(viewportRect, 3.0);
    EXPECT_FLOAT_EQ(exportRect.x, 15.0f);
    EXPECT_FLOAT_EQ(exportRect.y, 21.0f);
    EXPECT_FLOAT_EQ(exportRect.width, 60.0f);
    EXPECT_FLOAT_EQ(exportRect.height, 30.0f);
}

TEST(RenderViewGeometry, FixedOverlayResyncKeepsRendererTruthAcrossMixedDprRerenders) {
    FixedElementInfo fixed;
    fixed.viewport_x = 12.0f;
    fixed.viewport_y = 7.0f;
    fixed.pixel_width = 17;
    fixed.pixel_height = 11;
    fixed.renderer_dpr = 1.5f;
    fixed.logical_width = 11.0f;
    fixed.logical_height = 7.0f;

    ASSERT_GT(render_view_snapshot_logical_extent(fixed.pixel_width, fixed.renderer_dpr) - fixed.logical_width,
              0.3f);
    ASSERT_GT(render_view_snapshot_logical_extent(fixed.pixel_height, fixed.renderer_dpr) - fixed.logical_height,
              0.3f);

    render_view_resync_fixed_overlay_geometry(fixed, 2.0);

    EXPECT_FLOAT_EQ(fixed.viewport_x, 12.0f);
    EXPECT_FLOAT_EQ(fixed.viewport_y, 7.0f);
    EXPECT_FLOAT_EQ(fixed.logical_width, 11.0f);
    EXPECT_FLOAT_EQ(fixed.logical_height, 7.0f);

    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_fixed_overlay_viewport_rect(fixed, &viewportRect));
    RenderViewOverlayRect exportRect = render_view_scale_overlay_rect(viewportRect, 1.75);
    EXPECT_FLOAT_EQ(exportRect.width, 19.25f);
    EXPECT_FLOAT_EQ(exportRect.height, 12.25f);
}

TEST(RenderViewGeometry, DocumentXToRendererXUsesRenderedSliceOrigin) {
    EXPECT_FLOAT_EQ(render_view_document_x_to_renderer_x(125.0, 100.0, 2.0), 50.0f);
    EXPECT_FLOAT_EQ(render_view_document_x_to_renderer_x(80.0, 100.0, 2.0), -40.0f);
}

TEST(RenderViewGeometry, DocumentYToRendererYUsesRendererScale) {
    EXPECT_FLOAT_EQ(render_view_document_y_to_renderer_y(32.0, 2.5), 80.0f);
}

TEST(RenderViewGeometry, InputOverlayViewportRectUsesRendererScaleAndScrollOffsets) {
    clever::paint::Rect inputBounds{45.6f, 16.2f, 64.2f, 24.6f};
    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_input_overlay_viewport_rect(inputBounds,
                                                                33.25,
                                                                2.0,
                                                                1.25,
                                                                10.0,
                                                                6.0,
                                                                &viewportRect));
    EXPECT_NEAR(viewportRect.x, 47.1875, 0.001);
    EXPECT_NEAR(viewportRect.y, 14.0, 0.001);
    EXPECT_NEAR(viewportRect.width, 80.0, 0.001);
    EXPECT_NEAR(viewportRect.height, 31.25, 0.001);
}

TEST(RenderViewOverlay, InputOverlayViewportRectTracksRenderedSliceOrigin) {
    clever::paint::Rect inputBounds{45.6f, 16.2f, 64.2f, 24.6f};
    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_input_overlay_viewport_rect(inputBounds,
                                                                33.25,
                                                                2.0,
                                                                1.0,
                                                                0.0,
                                                                0.0,
                                                                &viewportRect));
    EXPECT_NEAR(viewportRect.x, 45.75, 0.001);
    EXPECT_NEAR(viewportRect.y, 16.0, 0.001);
    EXPECT_NEAR(viewportRect.width, 64.0, 0.001);
    EXPECT_NEAR(viewportRect.height, 25.0, 0.001);
}

TEST(RenderViewOverlay, InputOverlayViewportRectTracksVerticalScrollAndZoom) {
    clever::paint::Rect inputBounds{80.0f, 60.0f, 90.0f, 28.0f};
    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_input_overlay_viewport_rect(inputBounds,
                                                                0.0,
                                                                2.0,
                                                                1.5,
                                                                0.0,
                                                                22.0,
                                                                &viewportRect));
    EXPECT_NEAR(viewportRect.x, 120.0, 0.001);
    EXPECT_NEAR(viewportRect.y, 68.0, 0.001);
    EXPECT_NEAR(viewportRect.width, 135.0, 0.001);
    EXPECT_NEAR(viewportRect.height, 42.0, 0.001);
}

TEST(RenderViewOverlay, InputOverlayViewportRectResnapsAcrossRendererDprTransitions) {
    clever::paint::Rect inputBounds{45.6f, 16.2f, 64.2f, 24.6f};
    RenderViewOverlayRect dpr1Rect;
    ASSERT_TRUE(render_view_compute_input_overlay_viewport_rect(inputBounds,
                                                                33.25,
                                                                1.0,
                                                                1.0,
                                                                0.0,
                                                                0.0,
                                                                &dpr1Rect));
    EXPECT_NEAR(dpr1Rect.x, 45.25, 0.001);
    EXPECT_NEAR(dpr1Rect.y, 16.0, 0.001);
    EXPECT_NEAR(dpr1Rect.width, 65.0, 0.001);
    EXPECT_NEAR(dpr1Rect.height, 25.0, 0.001);

    RenderViewOverlayRect dpr2Rect;
    ASSERT_TRUE(render_view_compute_input_overlay_viewport_rect(inputBounds,
                                                                33.25,
                                                                2.0,
                                                                1.0,
                                                                0.0,
                                                                0.0,
                                                                &dpr2Rect));
    EXPECT_NEAR(dpr2Rect.x, 45.75, 0.001);
    EXPECT_NEAR(dpr2Rect.y, 16.0, 0.001);
    EXPECT_NEAR(dpr2Rect.width, 64.0, 0.001);
    EXPECT_NEAR(dpr2Rect.height, 25.0, 0.001);
}

TEST(RenderViewHorizontalScroll, DocumentAndRendererConversionStayAlignedAcrossWideSlices) {
    const CGFloat pageScale = 1.5;
    const CGFloat scrollOffsetX = 180.0;
    const CGFloat scrollOffsetY = 30.0;
    const CGFloat renderedOriginX = 96.0;
    const CGFloat rendererScale = 2.0;

    const CGFloat documentX = (18.0 + scrollOffsetX) / pageScale;
    const CGFloat documentY = (12.0 + scrollOffsetY) / pageScale;
    EXPECT_NEAR(documentX, 132.0, 0.001);
    EXPECT_NEAR(documentY, 28.0, 0.001);
    EXPECT_NEAR(render_view_document_x_to_renderer_x(documentX, renderedOriginX, rendererScale),
                72.0,
                0.001);
    EXPECT_NEAR(render_view_document_y_to_renderer_y(documentY, rendererScale),
                56.0,
                0.001);

    RenderViewOverlayRect rect;
    ASSERT_TRUE(render_view_compute_input_overlay_viewport_rect(
        clever::paint::Rect{120.0f, 24.0f, 32.0f, 10.0f},
        renderedOriginX,
        rendererScale,
        pageScale,
        scrollOffsetX,
        scrollOffsetY,
        &rect));
    EXPECT_NEAR(rect.x, 0.0, 0.001);
    EXPECT_NEAR(rect.y, 6.0, 0.001);
    EXPECT_NEAR(rect.width, 48.0, 0.001);
    EXPECT_NEAR(rect.height, 15.0, 0.001);
}

TEST(RenderViewHorizontalOverlay, InputViewportRectTracksWideContentScrollZoomAndOrigin) {
    clever::paint::Rect inputBounds{45.6f, 16.2f, 64.2f, 24.6f};
    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_input_overlay_viewport_rect(inputBounds,
                                                                33.25,
                                                                2.0,
                                                                1.25,
                                                                10.0,
                                                                6.0,
                                                                &viewportRect));
    EXPECT_NEAR(viewportRect.x, 47.1875, 0.001);
    EXPECT_NEAR(viewportRect.y, 14.0, 0.001);
    EXPECT_NEAR(viewportRect.width, 80.0, 0.001);
    EXPECT_NEAR(viewportRect.height, 31.25, 0.001);

    RenderViewOverlayRect dprShiftedRect;
    ASSERT_TRUE(render_view_compute_input_overlay_viewport_rect(inputBounds,
                                                                33.25,
                                                                1.0,
                                                                1.25,
                                                                10.0,
                                                                6.0,
                                                                &dprShiftedRect));
    EXPECT_NEAR(dprShiftedRect.x, 46.5625, 0.001);
    EXPECT_NEAR(dprShiftedRect.y, 14.0, 0.001);
    EXPECT_NEAR(dprShiftedRect.width, 81.25, 0.001);
    EXPECT_NEAR(dprShiftedRect.height, 31.25, 0.001);
}

TEST(RenderViewNestedScrollOverlay, ChildScrollOffsetsKeepOverlayAndViewportGeometryAligned) {
    StickyElementInfo sticky;
    sticky.abs_y = 100.0f;
    sticky.height = 12.0f;
    sticky.top_offset = 8.0f;
    sticky.container_top = 60.0f;
    sticky.container_bottom = 160.0f;
    sticky.container_scroll_x = 30.0f;
    sticky.container_scroll_y = 36.0f;
    sticky.container_x = 50.0f;
    sticky.container_y = 60.0f;
    sticky.is_page_sticky = false;
    sticky.pixel_width = 60;
    sticky.pixel_height = 24;
    sticky.renderer_dpr = 2.0f;
    sticky.logical_x = 120.0f;
    sticky.logical_width = 30.0f;
    sticky.logical_height = 12.0f;

    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_sticky_overlay_viewport_rect(
        sticky,
        20.0f,
        10.0f,
        0.0,
        400.0f,
        &viewportRect));
    EXPECT_FLOAT_EQ(viewportRect.x, 70.0f);
    EXPECT_FLOAT_EQ(viewportRect.y, 58.0f);
    EXPECT_FLOAT_EQ(viewportRect.width, 30.0f);
    EXPECT_FLOAT_EQ(viewportRect.height, 12.0f);

    RenderViewOverlayRect exportRect = render_view_scale_overlay_rect(viewportRect, 2.0);
    EXPECT_FLOAT_EQ(exportRect.x, 140.0f);
    EXPECT_FLOAT_EQ(exportRect.y, 116.0f);
    EXPECT_FLOAT_EQ(exportRect.width, 60.0f);
    EXPECT_FLOAT_EQ(exportRect.height, 24.0f);

    const float localCssX = 9.0f;
    const float localCssY = 5.0f;
    const float overlayDocumentX = viewportRect.x + 20.0f + sticky.container_scroll_x + localCssX;
    const float overlayDocumentY = sticky.abs_y + localCssY;
    EXPECT_FLOAT_EQ(overlayDocumentX, 129.0f);
    EXPECT_FLOAT_EQ(overlayDocumentY, 105.0f);

    const float viewportSourceX = render_view_document_x_to_renderer_x(20.0f, 0.0f, 2.0f);
    const float overlayViewportPixelX =
        render_view_document_x_to_renderer_x(overlayDocumentX, 0.0f, 2.0f) -
        viewportSourceX -
        sticky.container_scroll_x * 2.0f;
    EXPECT_FLOAT_EQ(overlayViewportPixelX, exportRect.x + localCssX * 2.0f);
    EXPECT_FLOAT_EQ(exportRect.y + localCssY * 2.0f, 126.0f);
}

TEST(NativeImageDecode, StickyInnerScrollerThresholdIsInclusive) {
    StickyElementInfo sticky;
    sticky.abs_y = 100.0f;
    sticky.height = 12.0f;
    sticky.top_offset = 8.0f;
    sticky.container_bottom = 160.0f;
    sticky.container_scroll_x = 30.0f;
    sticky.container_scroll_y = 32.0f;
    sticky.container_x = 50.0f;
    sticky.container_y = 60.0f;
    sticky.is_page_sticky = false;
    sticky.pixel_width = 60;
    sticky.pixel_height = 24;
    sticky.renderer_dpr = 2.0f;
    sticky.logical_x = 120.0f;
    sticky.logical_width = 30.0f;
    sticky.logical_height = 12.0f;

    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_sticky_overlay_viewport_rect(
        sticky,
        20.0f,
        10.0f,
        0.0,
        400.0f,
        &viewportRect));
    EXPECT_FLOAT_EQ(viewportRect.x, 70.0f);
    EXPECT_FLOAT_EQ(viewportRect.y, 58.0f);
    EXPECT_FLOAT_EQ(viewportRect.width, 30.0f);
    EXPECT_FLOAT_EQ(viewportRect.height, 12.0f);
}

TEST(NativeImageDecode, StickyNestedSnapshotUsesContainerClipBoundary) {
    StickyElementInfo sticky;
    sticky.abs_y = 100.0f;
    sticky.height = 12.0f;
    sticky.top_offset = 8.0f;
    sticky.container_bottom = 160.0f;
    sticky.container_scroll_x = 30.0f;
    sticky.container_scroll_y = 36.0f;
    sticky.container_x = 50.0f;
    sticky.container_y = 60.0f;
    sticky.is_page_sticky = false;
    sticky.pixel_width = 20;
    sticky.pixel_height = 24;
    sticky.renderer_dpr = 2.0f;
    sticky.logical_x = 60.0f;
    sticky.logical_width = 40.0f;
    sticky.logical_height = 12.0f;

    render_view_resync_sticky_overlay_geometry(sticky, 3.0);

    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_sticky_overlay_viewport_rect(
        sticky,
        20.0f,
        10.0f,
        0.0,
        400.0f,
        &viewportRect));
    EXPECT_FLOAT_EQ(viewportRect.x, 30.0f);
    EXPECT_FLOAT_EQ(viewportRect.y, 58.0f);
    EXPECT_FLOAT_EQ(viewportRect.width, 10.0f);
    EXPECT_FLOAT_EQ(viewportRect.height, 12.0f);

    RenderViewOverlayRect exportRect = render_view_scale_overlay_rect(viewportRect, 1.5);
    EXPECT_FLOAT_EQ(exportRect.x, 45.0f);
    EXPECT_FLOAT_EQ(exportRect.y, 87.0f);
    EXPECT_FLOAT_EQ(exportRect.width, 15.0f);
    EXPECT_FLOAT_EQ(exportRect.height, 18.0f);
}

TEST(NativeImageDecode, StickySliceOriginStaysAlignedAcrossDprAndScaleChanges) {
    StickyElementInfo sticky;
    sticky.abs_y = 60.0f;
    sticky.height = 10.0f;
    sticky.top_offset = 4.0f;
    sticky.pixel_width = 20;
    sticky.pixel_height = 20;
    sticky.renderer_dpr = 2.0f;
    sticky.logical_x = 70.0f;
    sticky.logical_width = 20.0f;
    sticky.logical_height = 10.0f;

    render_view_resync_sticky_overlay_geometry(sticky, 3.0);

    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_sticky_overlay_viewport_rect(
        sticky,
        60.0f,
        80.0f,
        72.0,
        400.0f,
        &viewportRect));
    EXPECT_FLOAT_EQ(viewportRect.x, 12.0f);
    EXPECT_FLOAT_EQ(viewportRect.y, 4.0f);
    EXPECT_FLOAT_EQ(viewportRect.width, 10.0f);
    EXPECT_FLOAT_EQ(viewportRect.height, 10.0f);

    RenderViewOverlayRect exportRect = render_view_scale_overlay_rect(viewportRect, 1.5);
    EXPECT_FLOAT_EQ(exportRect.x, 18.0f);
    EXPECT_FLOAT_EQ(exportRect.y, 6.0f);
    EXPECT_FLOAT_EQ(exportRect.width, 15.0f);
    EXPECT_FLOAT_EQ(exportRect.height, 15.0f);
}

TEST(RenderViewGeometry, StickyOverlayKeepsRendererTruthAcrossMixedDprRerenders) {
    StickyElementInfo sticky;
    sticky.abs_y = 40.0f;
    sticky.height = 7.0f;
    sticky.top_offset = 5.0f;
    sticky.pixel_width = 17;
    sticky.pixel_height = 11;
    sticky.renderer_dpr = 1.5f;
    sticky.logical_x = 64.0f;
    sticky.logical_width = 11.0f;
    sticky.logical_height = 7.0f;

    ASSERT_GT(render_view_snapshot_logical_extent(sticky.pixel_width, sticky.renderer_dpr) - sticky.logical_width,
              0.3f);
    ASSERT_GT(render_view_snapshot_logical_extent(sticky.pixel_height, sticky.renderer_dpr) - sticky.logical_height,
              0.3f);

    render_view_resync_sticky_overlay_geometry(sticky, 2.0);

    RenderViewOverlayRect viewportRect;
    ASSERT_TRUE(render_view_compute_sticky_overlay_viewport_rect(
        sticky,
        60.0f,
        40.0f,
        60.0,
        200.0f,
        &viewportRect));
    EXPECT_FLOAT_EQ(viewportRect.x, 4.0f);
    EXPECT_FLOAT_EQ(viewportRect.y, 5.0f);
    EXPECT_FLOAT_EQ(viewportRect.width, 11.0f);
    EXPECT_FLOAT_EQ(viewportRect.height, 7.0f);

    RenderViewOverlayRect exportRect = render_view_scale_overlay_rect(viewportRect, 1.75);
    EXPECT_FLOAT_EQ(exportRect.x, 7.0f);
    EXPECT_FLOAT_EQ(exportRect.y, 8.75f);
    EXPECT_FLOAT_EQ(exportRect.width, 19.25f);
    EXPECT_FLOAT_EQ(exportRect.height, 12.25f);
}

TEST(RenderViewGeometry, PageImageViewportRectUsesRendererLogicalSizeAcrossMixedDprAndZoom) {
    RenderViewOverlayRect pageRect;
    ASSERT_TRUE(render_view_compute_page_image_viewport_rect(33.25,
                                                             101.0,
                                                             67.0,
                                                             1.75,
                                                             20.0,
                                                             12.0,
                                                             &pageRect));
    EXPECT_NEAR(pageRect.x, 38.1875, 0.001);
    EXPECT_NEAR(pageRect.y, -12.0, 0.001);
    EXPECT_NEAR(pageRect.width, 176.75, 0.001);
    EXPECT_NEAR(pageRect.height, 117.25, 0.001);

    const CGFloat staleWidth = (152.0 / 1.5) * 1.75;
    const CGFloat staleHeight = (101.0 / 1.5) * 1.75;
    EXPECT_GT(std::abs(pageRect.width - staleWidth), 0.5);
    EXPECT_GT(std::abs(pageRect.height - staleHeight), 0.5);
}

TEST(BrowserWindowRetinaAnchor, ProgrammaticScrollSnapsToRendererLogicalGridAcrossZoom) {
    const CGFloat rendererScale = 2.0;
    const BrowserWindowLogicalPoint scrollPoint =
        browser_window_normalize_document_scroll_point(121.26, 48.26, rendererScale);

    EXPECT_NEAR(scrollPoint.x, 121.5, 0.001);
    EXPECT_NEAR(scrollPoint.y, 48.5, 0.001);
    EXPECT_NEAR(render_view_document_x_to_renderer_x(scrollPoint.x, 0.0, rendererScale),
                std::round(render_view_document_x_to_renderer_x(121.26, 0.0, rendererScale)),
                0.001);
    EXPECT_NEAR(render_view_document_y_to_renderer_y(scrollPoint.y, rendererScale),
                std::round(render_view_document_y_to_renderer_y(48.26, rendererScale)),
                0.001);

    const CGFloat pageScale = 1.75;
    const CGFloat viewOffsetX = scrollPoint.x * pageScale;
    const CGFloat viewOffsetY = scrollPoint.y * pageScale;
    EXPECT_NEAR(viewOffsetX / pageScale, scrollPoint.x, 0.001);
    EXPECT_NEAR(viewOffsetY / pageScale, scrollPoint.y, 0.001);
}

TEST(BrowserWindowRetinaHitTest, CoordinatesSnapRelativeToRenderedSliceOrigin) {
    const CGFloat renderedOriginX = 120.5;
    const CGFloat rendererScale = 2.0;
    const BrowserWindowLogicalPoint hitPoint =
        browser_window_normalize_document_hit_test_point(133.26, 64.26, renderedOriginX, rendererScale);

    EXPECT_NEAR(hitPoint.x, 133.5, 0.001);
    EXPECT_NEAR(hitPoint.y, 64.5, 0.001);
    EXPECT_NEAR(render_view_document_x_to_renderer_x(hitPoint.x, renderedOriginX, rendererScale),
                std::round(render_view_document_x_to_renderer_x(133.26, renderedOriginX, rendererScale)),
                0.001);
    EXPECT_NEAR(render_view_document_y_to_renderer_y(hitPoint.y, rendererScale),
                std::round(render_view_document_y_to_renderer_y(64.26, rendererScale)),
                0.001);
}

TEST(BrowserWindowAnchorHitTestRetina, FollowUpClickUsesSameLogicalGridAfterAnchorScroll) {
    const CGFloat rendererScale = 2.0;
    const CGFloat pageScale = 1.5;
    const BrowserWindowLogicalPoint anchorScroll =
        browser_window_normalize_document_scroll_point(120.26, 48.26, rendererScale);

    const CGFloat localViewX = 17.0;
    const CGFloat localViewY = 11.0;
    const CGFloat rawDocumentX = anchorScroll.x + localViewX / pageScale;
    const CGFloat rawDocumentY = anchorScroll.y + localViewY / pageScale;
    const BrowserWindowLogicalPoint hitPoint =
        browser_window_normalize_document_hit_test_point(rawDocumentX,
                                                         rawDocumentY,
                                                         anchorScroll.x,
                                                         rendererScale);

    EXPECT_NEAR(hitPoint.x,
                anchorScroll.x + std::round((localViewX / pageScale) * rendererScale) / rendererScale,
                0.001);
    EXPECT_NEAR(hitPoint.y,
                anchorScroll.y + std::round((localViewY / pageScale) * rendererScale) / rendererScale,
                0.001);
    EXPECT_NEAR(render_view_document_x_to_renderer_x(hitPoint.x, anchorScroll.x, rendererScale),
                std::round(render_view_document_x_to_renderer_x(rawDocumentX,
                                                                anchorScroll.x,
                                                                rendererScale)),
                0.001);
    EXPECT_NEAR((hitPoint.y - anchorScroll.y) * rendererScale,
                std::round((rawDocumentY - anchorScroll.y) * rendererScale),
                0.001);
}

TEST(BrowserWindowRetinaAnchor, ProgrammaticScrollReSnapsWhenRendererDprChanges) {
    const BrowserWindowLogicalPoint dpr2Scroll =
        browser_window_normalize_document_scroll_point(120.26, 48.26, 2.0);
    const BrowserWindowLogicalPoint dpr3Scroll =
        browser_window_normalize_document_scroll_point(120.26, 48.26, 3.0);

    EXPECT_NEAR(dpr2Scroll.x, 120.5, 0.001);
    EXPECT_NEAR(dpr2Scroll.y, 48.5, 0.001);
    EXPECT_NEAR(dpr3Scroll.x, 120.333333, 0.001);
    EXPECT_NEAR(dpr3Scroll.y, 48.333333, 0.001);
    EXPECT_GT(std::abs(dpr2Scroll.x - dpr3Scroll.x), 0.1);
    EXPECT_GT(std::abs(dpr2Scroll.y - dpr3Scroll.y), 0.1);

    EXPECT_NEAR(render_view_document_x_to_renderer_x(dpr3Scroll.x, 0.0, 3.0),
                std::round(render_view_document_x_to_renderer_x(120.26, 0.0, 3.0)),
                0.001);
    EXPECT_NEAR(render_view_document_y_to_renderer_y(dpr3Scroll.y, 3.0),
                std::round(render_view_document_y_to_renderer_y(48.26, 3.0)),
                0.001);

    const CGFloat pageScale = 1.75;
    const CGFloat viewOffsetX = dpr3Scroll.x * pageScale;
    const CGFloat viewOffsetY = dpr3Scroll.y * pageScale;
    EXPECT_NEAR(viewOffsetX / pageScale, dpr3Scroll.x, 0.001);
    EXPECT_NEAR(viewOffsetY / pageScale, dpr3Scroll.y, 0.001);
}

TEST(BrowserWindowRetinaHitTest, MixedZoomUsesRenderedSliceOriginInsteadOfScrollOffsetOrigin) {
    const CGFloat pageScale = 1.75;
    const CGFloat rendererScale = 2.0;
    const CGFloat renderedOriginX = 96.5;
    const CGFloat scrollDocumentX = 120.25;
    const CGFloat scrollDocumentY = 48.25;
    const CGFloat localViewX = 17.0;
    const CGFloat localViewY = 11.0;

    const CGFloat rawDocumentX = scrollDocumentX + localViewX / pageScale;
    const CGFloat rawDocumentY = scrollDocumentY + localViewY / pageScale;
    const BrowserWindowLogicalPoint renderedOriginHit =
        browser_window_normalize_document_hit_test_point(rawDocumentX,
                                                         rawDocumentY,
                                                         renderedOriginX,
                                                         rendererScale);
    const BrowserWindowLogicalPoint scrollOriginHit =
        browser_window_normalize_document_hit_test_point(rawDocumentX,
                                                         rawDocumentY,
                                                         scrollDocumentX,
                                                         rendererScale);

    EXPECT_NEAR(renderedOriginHit.x, 130.0, 0.001);
    EXPECT_NEAR(renderedOriginHit.y, 54.5, 0.001);
    EXPECT_NEAR(render_view_document_x_to_renderer_x(renderedOriginHit.x,
                                                     renderedOriginX,
                                                     rendererScale),
                std::round(render_view_document_x_to_renderer_x(rawDocumentX,
                                                                renderedOriginX,
                                                                rendererScale)),
                0.001);
    EXPECT_NEAR(render_view_document_y_to_renderer_y(renderedOriginHit.y, rendererScale),
                std::round(render_view_document_y_to_renderer_y(rawDocumentY, rendererScale)),
                0.001);
    EXPECT_GT(std::abs(renderedOriginHit.x - scrollOriginHit.x), 0.2);
    EXPECT_NEAR(render_view_document_x_to_renderer_x(renderedOriginHit.x,
                                                     renderedOriginX,
                                                     rendererScale),
                67.0,
                0.001);
}
