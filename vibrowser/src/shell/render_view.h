#pragma once
#import <Cocoa/Cocoa.h>
#include <clever/paint/display_list.h>
#include <clever/paint/render_pipeline.h>
#include <clever/paint/software_renderer.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

// Info about a position:sticky element extracted after layout + render.
// Stores the element's absolute page position, its CSS top offset, the
// scrollable container bounds, and a copy of the rendered pixels so we
// can composite the element at the "stuck" position during scroll.
struct StickyElementInfo {
    float abs_y = 0;           // absolute Y of the element in page coordinates (CSS pixels)
    float height = 0;          // border-box height of the element (CSS pixels)
    float top_offset = 0;      // CSS `top` value in CSS pixels
    float container_top = 0;   // top of the scrollable container (CSS pixels)
    float container_bottom = 0;// bottom of the scrollable container (CSS pixels)
    float container_scroll_x = 0; // scroll_left of the sticky container (CSS pixels)
    float container_scroll_y = 0; // scroll_top of the sticky container (CSS pixels)
    float container_x = 0;     // x position of the scroll container in page coordinates (CSS pixels)
    float container_y = 0;     // y position of the scroll container in page coordinates (CSS pixels)
    bool is_page_sticky = true;   // true when relative to viewport (page scroll), false for container-relative sticky
    // Pixel data for the sticky element's region (RGBA, row-major)
    std::vector<uint8_t> pixels;
    int pixel_x = 0;           // x position in the rendered buffer
    int pixel_width = 0;       // width in pixels
    int pixel_height = 0;      // height in pixels
    float renderer_dpr = 1.0f; // DPR used when this overlay snapshot was captured
    float logical_x = 0;       // overlay x in CSS pixels
    float logical_width = 0;   // overlay width in CSS pixels
    float logical_height = 0;  // overlay height in CSS pixels
};

// Info about a position:fixed element extracted after layout + render.
// Fixed elements are positioned relative to the viewport and must not
// move when the page scrolls. We extract their rendered pixels and
// composite them at their viewport-relative position during drawRect.
struct FixedElementInfo {
    // Viewport-relative position in CSS/page coordinates.
    float viewport_x = 0;      // X position in viewport (CSS pixels)
    float viewport_y = 0;      // Y position in viewport (CSS pixels)
    // Pixel data for the fixed element's region (RGBA, row-major)
    std::vector<uint8_t> pixels;
    int pixel_width = 0;       // width in pixels
    int pixel_height = 0;      // height in pixels
    float renderer_dpr = 1.0f; // DPR used when this overlay snapshot was captured
    float logical_width = 0;   // overlay width in CSS pixels
    float logical_height = 0;  // overlay height in CSS pixels
};

struct RenderViewOverlayRect {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
};

static inline CGFloat render_view_normalized_renderer_scale(CGFloat candidateScale,
                                                            CGFloat fallbackScale = 1.0) {
    if (std::isfinite(candidateScale) && candidateScale >= 1.0) {
        return candidateScale;
    }
    if (std::isfinite(fallbackScale) && fallbackScale >= 1.0) {
        return fallbackScale;
    }
    return 1.0;
}

static inline CGFloat render_view_normalized_view_scale(CGFloat candidateScale,
                                                        CGFloat fallbackScale = 1.0) {
    if (std::isfinite(candidateScale) && candidateScale > 0.0) {
        return candidateScale;
    }
    if (std::isfinite(fallbackScale) && fallbackScale > 0.0) {
        return fallbackScale;
    }
    return 1.0;
}

static inline float render_view_snapshot_logical_extent(int pixelExtent,
                                                        CGFloat overlayScale) {
    if (pixelExtent <= 0 || !std::isfinite(overlayScale) || overlayScale <= 0.0) {
        return 0.0f;
    }
    return static_cast<float>(pixelExtent / overlayScale);
}

static inline bool render_view_snapshot_extent_is_clipped(float originalExtent,
                                                          float snapshotExtent,
                                                          CGFloat overlayScale) {
    if (!(originalExtent > 0.0f) || !(snapshotExtent > 0.0f)) {
        return false;
    }
    const float extentTolerance = static_cast<float>(0.5 / overlayScale + 0.01);
    return snapshotExtent + extentTolerance < originalExtent;
}

static inline float render_view_document_x_to_renderer_x(CGFloat documentX,
                                                         CGFloat renderedDocumentOriginX,
                                                         CGFloat rendererScale) {
    const CGFloat normalizedScale = render_view_normalized_renderer_scale(rendererScale);
    const CGFloat normalizedOrigin = std::isfinite(renderedDocumentOriginX)
        ? renderedDocumentOriginX
        : 0.0;
    return static_cast<float>((documentX - normalizedOrigin) * normalizedScale);
}

static inline float render_view_document_y_to_renderer_y(CGFloat documentY,
                                                         CGFloat rendererScale) {
    const CGFloat normalizedScale = render_view_normalized_renderer_scale(rendererScale);
    return static_cast<float>(documentY * normalizedScale);
}

static inline bool render_view_compute_input_overlay_viewport_rect(
    const clever::paint::Rect& documentBounds,
    CGFloat renderedDocumentOriginX,
    CGFloat rendererScale,
    CGFloat pageScale,
    CGFloat scrollOffsetX,
    CGFloat scrollOffsetY,
    RenderViewOverlayRect* outRect) {
    if (outRect) {
        *outRect = {};
    }
    if (!(documentBounds.width > 0.0f) || !(documentBounds.height > 0.0f)) {
        return false;
    }
    if (!std::isfinite(documentBounds.x) || !std::isfinite(documentBounds.y) ||
        !std::isfinite(documentBounds.width) || !std::isfinite(documentBounds.height)) {
        return false;
    }

    const CGFloat normalizedRendererScale = render_view_normalized_renderer_scale(rendererScale);
    const CGFloat normalizedPageScale = render_view_normalized_view_scale(pageScale);
    const CGFloat normalizedOriginX = std::isfinite(renderedDocumentOriginX)
        ? renderedDocumentOriginX
        : 0.0;
    const CGFloat normalizedScrollX = std::isfinite(scrollOffsetX) ? scrollOffsetX : 0.0;
    const CGFloat normalizedScrollY = std::isfinite(scrollOffsetY) ? scrollOffsetY : 0.0;

    const CGFloat rendererLeft = std::lround(
        render_view_document_x_to_renderer_x(documentBounds.x, normalizedOriginX, normalizedRendererScale));
    const CGFloat rendererTop = std::lround(
        render_view_document_y_to_renderer_y(documentBounds.y, normalizedRendererScale));
    const CGFloat rendererRight = std::lround(
        render_view_document_x_to_renderer_x(documentBounds.x + documentBounds.width,
                                             normalizedOriginX,
                                             normalizedRendererScale));
    const CGFloat rendererBottom = std::lround(
        render_view_document_y_to_renderer_y(documentBounds.y + documentBounds.height,
                                             normalizedRendererScale));

    CGFloat snappedX = normalizedOriginX + rendererLeft / normalizedRendererScale;
    CGFloat snappedY = rendererTop / normalizedRendererScale;
    CGFloat snappedWidth = (rendererRight - rendererLeft) / normalizedRendererScale;
    CGFloat snappedHeight = (rendererBottom - rendererTop) / normalizedRendererScale;
    if (!(snappedWidth > 0.0) || !(snappedHeight > 0.0)) {
        snappedX = documentBounds.x;
        snappedY = documentBounds.y;
        snappedWidth = documentBounds.width;
        snappedHeight = documentBounds.height;
    }

    RenderViewOverlayRect rect;
    rect.x = static_cast<float>(snappedX * normalizedPageScale - normalizedScrollX);
    rect.y = static_cast<float>(snappedY * normalizedPageScale - normalizedScrollY);
    rect.width = static_cast<float>(snappedWidth * normalizedPageScale);
    rect.height = static_cast<float>(snappedHeight * normalizedPageScale);
    if (outRect) {
        *outRect = rect;
    }
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
           rect.width > 0.0f && rect.height > 0.0f;
}

static inline bool render_view_extract_buffer_rect(const clever::paint::Rect& cssBounds,
                                                   CGFloat rendererScale,
                                                   int bufferWidth,
                                                   int bufferHeight,
                                                   int* outX,
                                                   int* outY,
                                                   int* outWidth,
                                                   int* outHeight) {
    if (outX) *outX = 0;
    if (outY) *outY = 0;
    if (outWidth) *outWidth = 0;
    if (outHeight) *outHeight = 0;

    if (bufferWidth <= 0 || bufferHeight <= 0) {
        return false;
    }

    const CGFloat normalizedScale = render_view_normalized_renderer_scale(rendererScale);
    const int rawLeft = static_cast<int>(std::lround(cssBounds.x * normalizedScale));
    const int rawTop = static_cast<int>(std::lround(cssBounds.y * normalizedScale));
    const int rawRight = static_cast<int>(std::lround((cssBounds.x + cssBounds.width) * normalizedScale));
    const int rawBottom = static_cast<int>(std::lround((cssBounds.y + cssBounds.height) * normalizedScale));

    const int clippedLeft = std::max(0, std::min(rawLeft, bufferWidth));
    const int clippedTop = std::max(0, std::min(rawTop, bufferHeight));
    const int clippedRight = std::max(0, std::min(rawRight, bufferWidth));
    const int clippedBottom = std::max(0, std::min(rawBottom, bufferHeight));

    const int clippedWidth = std::max(0, clippedRight - clippedLeft);
    const int clippedHeight = std::max(0, clippedBottom - clippedTop);
    if (clippedWidth <= 0 || clippedHeight <= 0) {
        return false;
    }

    if (outX) *outX = clippedLeft;
    if (outY) *outY = clippedTop;
    if (outWidth) *outWidth = clippedWidth;
    if (outHeight) *outHeight = clippedHeight;
    return true;
}

static inline void render_view_resync_sticky_overlay_geometry(StickyElementInfo& elem,
                                                              CGFloat fallbackScale = 1.0) {
    const CGFloat overlayScale =
        render_view_normalized_renderer_scale(static_cast<CGFloat>(elem.renderer_dpr), fallbackScale);
    elem.renderer_dpr = static_cast<float>(overlayScale);

    if (elem.pixel_width <= 0 || elem.pixel_height <= 0) {
        return;
    }

    const float snapshotLogicalWidth =
        render_view_snapshot_logical_extent(elem.pixel_width, overlayScale);
    const float snapshotLogicalHeight =
        render_view_snapshot_logical_extent(elem.pixel_height, overlayScale);

    // Sticky snapshots are extracted from whatever renderer buffer is current,
    // including horizontally scrolled viewport slices. Their stored logical
    // origin is already document-space; rebasing from pixel_x makes the overlay
    // jump when the slice origin or DPR changes.
    if (!std::isfinite(elem.logical_x)) {
        elem.logical_x = static_cast<float>(elem.pixel_x / overlayScale);
    }
    if (!(elem.logical_width > 0.0f)) {
        elem.logical_width = snapshotLogicalWidth;
    }
    if (!(elem.logical_height > 0.0f)) {
        elem.logical_height = snapshotLogicalHeight;
    }
}

static inline void render_view_resync_fixed_overlay_geometry(FixedElementInfo& elem,
                                                             CGFloat fallbackScale = 1.0) {
    const CGFloat overlayScale =
        render_view_normalized_renderer_scale(static_cast<CGFloat>(elem.renderer_dpr), fallbackScale);
    elem.renderer_dpr = static_cast<float>(overlayScale);

    if (elem.pixel_width <= 0 || elem.pixel_height <= 0) {
        return;
    }

    const float snapshotLogicalWidth =
        render_view_snapshot_logical_extent(elem.pixel_width, overlayScale);
    const float snapshotLogicalHeight =
        render_view_snapshot_logical_extent(elem.pixel_height, overlayScale);
    const bool hasLogicalWidth = std::isfinite(elem.logical_width) && elem.logical_width > 0.0f;
    const bool hasLogicalHeight = std::isfinite(elem.logical_height) && elem.logical_height > 0.0f;
    const bool clippedWidth =
        hasLogicalWidth &&
        render_view_snapshot_extent_is_clipped(elem.logical_width, snapshotLogicalWidth, overlayScale);
    const bool clippedHeight =
        hasLogicalHeight &&
        render_view_snapshot_extent_is_clipped(elem.logical_height, snapshotLogicalHeight, overlayScale);

    float visibleLogicalWidth = hasLogicalWidth ? elem.logical_width : snapshotLogicalWidth;
    float visibleLogicalHeight = hasLogicalHeight ? elem.logical_height : snapshotLogicalHeight;

    if (!std::isfinite(elem.viewport_x)) {
        elem.viewport_x = 0.0f;
    } else if (elem.viewport_x < 0.0f) {
        visibleLogicalWidth = hasLogicalWidth
            ? std::max(0.0f, elem.logical_width + elem.viewport_x)
            : snapshotLogicalWidth;
        elem.viewport_x = 0.0f;
    } else if (!hasLogicalWidth || clippedWidth) {
        visibleLogicalWidth = snapshotLogicalWidth;
    }

    if (!std::isfinite(elem.viewport_y)) {
        elem.viewport_y = 0.0f;
    } else if (elem.viewport_y < 0.0f) {
        visibleLogicalHeight = hasLogicalHeight
            ? std::max(0.0f, elem.logical_height + elem.viewport_y)
            : snapshotLogicalHeight;
        elem.viewport_y = 0.0f;
    } else if (!hasLogicalHeight || clippedHeight) {
        visibleLogicalHeight = snapshotLogicalHeight;
    }

    elem.logical_width = visibleLogicalWidth;
    elem.logical_height = visibleLogicalHeight;
}

static inline float render_view_compute_sticky_overlay_clip_boundary_x(
    const StickyElementInfo& elem,
    float scrollCssX,
    CGFloat renderedDocumentOriginX) {
    float clipBoundaryCss = std::max(
        std::isfinite(scrollCssX) ? scrollCssX : 0.0f,
        static_cast<float>(std::max<CGFloat>(
            0.0,
            std::isfinite(renderedDocumentOriginX) ? renderedDocumentOriginX : 0.0)));
    if (elem.is_page_sticky) {
        return clipBoundaryCss;
    }

    if (std::isfinite(elem.container_x) && std::isfinite(elem.container_scroll_x)) {
        clipBoundaryCss = std::max(clipBoundaryCss, elem.container_x + elem.container_scroll_x);
    } else if (std::isfinite(elem.container_scroll_x)) {
        clipBoundaryCss = std::max(clipBoundaryCss, scrollCssX + elem.container_scroll_x);
    }
    return clipBoundaryCss;
}

static inline bool render_view_compute_sticky_overlay_viewport_rect(
    const StickyElementInfo& elem,
    float scrollCssX,
    float scrollCssY,
    CGFloat renderedDocumentOriginX,
    float contentHeight,
    RenderViewOverlayRect* outRect) {
    if (outRect) {
        *outRect = {};
    }
    if (!(elem.logical_width > 0.0f) || !(elem.logical_height > 0.0f)) {
        return false;
    }
    if (!std::isfinite(scrollCssX) || !std::isfinite(scrollCssY) || !std::isfinite(contentHeight)) {
        return false;
    }

    const CGFloat overlayScale =
        render_view_normalized_renderer_scale(static_cast<CGFloat>(elem.renderer_dpr));
    const bool hasLogicalWidth = std::isfinite(elem.logical_width) && elem.logical_width > 0.0f;
    const bool hasLogicalHeight = std::isfinite(elem.logical_height) && elem.logical_height > 0.0f;
    const float snapshotLogicalWidth = (elem.pixel_width > 0)
        ? render_view_snapshot_logical_extent(elem.pixel_width, overlayScale)
        : elem.logical_width;
    const float snapshotLogicalHeight = (elem.pixel_height > 0)
        ? render_view_snapshot_logical_extent(elem.pixel_height, overlayScale)
        : elem.logical_height;
    if (!(snapshotLogicalWidth > 0.0f) || !(snapshotLogicalHeight > 0.0f)) {
        return false;
    }
    const float storedLogicalWidth = hasLogicalWidth ? elem.logical_width : snapshotLogicalWidth;
    const float storedLogicalHeight = hasLogicalHeight ? elem.logical_height : snapshotLogicalHeight;
    const bool clippedWidth =
        hasLogicalWidth &&
        render_view_snapshot_extent_is_clipped(storedLogicalWidth, snapshotLogicalWidth, overlayScale);
    const bool clippedHeight =
        hasLogicalHeight &&
        render_view_snapshot_extent_is_clipped(storedLogicalHeight, snapshotLogicalHeight, overlayScale);
    const float visibleLogicalWidth = clippedWidth ? snapshotLogicalWidth : storedLogicalWidth;
    const float visibleLogicalHeight = clippedHeight ? snapshotLogicalHeight : storedLogicalHeight;

    const float stickyScrollCss = elem.is_page_sticky ? scrollCssY : elem.container_scroll_y;
    float normalY = elem.abs_y;
    if (!elem.is_page_sticky) {
        normalY -= elem.container_y;
    }
    const float stickThreshold = elem.top_offset;
    const bool shouldStick = (stickyScrollCss + stickThreshold >= normalY);
    if (!shouldStick) {
        return false;
    }

    const float containerBottomCss = elem.is_page_sticky ? contentHeight : elem.container_bottom;
    const float maxStickY = containerBottomCss - elem.height;
    const bool pastContainer = elem.is_page_sticky
        ? (stickyScrollCss + stickThreshold > maxStickY)
        : (elem.container_y + stickThreshold > maxStickY);

    const float drawYCss = pastContainer
        ? maxStickY
        : (elem.is_page_sticky ? (stickyScrollCss + stickThreshold)
                               : (elem.container_y + stickThreshold));
    const float clipBoundaryCss = render_view_compute_sticky_overlay_clip_boundary_x(
        elem, scrollCssX, renderedDocumentOriginX);
    const float maxVisibleLogicalX = elem.logical_x + std::max(0.0f, storedLogicalWidth - snapshotLogicalWidth);
    const float visibleLogicalX = (snapshotLogicalWidth + 0.01f < storedLogicalWidth)
        ? std::min(std::max(elem.logical_x, clipBoundaryCss), maxVisibleLogicalX)
        : elem.logical_x;

    RenderViewOverlayRect rect;
    rect.x = visibleLogicalX - scrollCssX;
    if (!elem.is_page_sticky) {
        rect.x -= elem.container_scroll_x;
    }
    rect.y = drawYCss - scrollCssY;
    rect.width = visibleLogicalWidth;
    rect.height = visibleLogicalHeight;

    if (outRect) {
        *outRect = rect;
    }
    return true;
}

static inline bool render_view_compute_fixed_overlay_viewport_rect(
    const FixedElementInfo& elem,
    RenderViewOverlayRect* outRect) {
    if (outRect) {
        *outRect = {};
    }
    if (!(elem.logical_width > 0.0f) || !(elem.logical_height > 0.0f)) {
        return false;
    }

    RenderViewOverlayRect rect;
    rect.x = elem.viewport_x;
    rect.y = elem.viewport_y;
    rect.width = elem.logical_width;
    rect.height = elem.logical_height;
    if (outRect) {
        *outRect = rect;
    }
    return std::isfinite(rect.x) && std::isfinite(rect.y);
}

static inline RenderViewOverlayRect render_view_scale_overlay_rect(
    const RenderViewOverlayRect& rect,
    CGFloat scale,
    CGFloat fallbackScale = 1.0) {
    const CGFloat normalizedScale = render_view_normalized_view_scale(scale, fallbackScale);
    RenderViewOverlayRect scaledRect;
    scaledRect.x = rect.x * normalizedScale;
    scaledRect.y = rect.y * normalizedScale;
    scaledRect.width = rect.width * normalizedScale;
    scaledRect.height = rect.height * normalizedScale;
    return scaledRect;
}

static inline bool render_view_compute_page_image_viewport_rect(
    CGFloat renderedDocumentOriginX,
    CGFloat renderedLogicalWidth,
    CGFloat renderedLogicalHeight,
    CGFloat pageScale,
    CGFloat scrollOffsetX,
    CGFloat scrollOffsetY,
    RenderViewOverlayRect* outRect) {
    if (outRect) {
        *outRect = {};
    }
    if (!(renderedLogicalWidth > 0.0) || !(renderedLogicalHeight > 0.0)) {
        return false;
    }

    const CGFloat normalizedPageScale = render_view_normalized_view_scale(pageScale);
    const CGFloat normalizedOriginX = std::isfinite(renderedDocumentOriginX)
        ? renderedDocumentOriginX
        : 0.0;
    const CGFloat normalizedScrollX = std::isfinite(scrollOffsetX) ? scrollOffsetX : 0.0;
    const CGFloat normalizedScrollY = std::isfinite(scrollOffsetY) ? scrollOffsetY : 0.0;

    RenderViewOverlayRect rect;
    rect.x = static_cast<float>(normalizedOriginX * normalizedPageScale - normalizedScrollX);
    rect.y = static_cast<float>(-normalizedScrollY);
    rect.width = static_cast<float>(renderedLogicalWidth * normalizedPageScale);
    rect.height = static_cast<float>(renderedLogicalHeight * normalizedPageScale);
    if (outRect) {
        *outRect = rect;
    }
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
           rect.width > 0.0f && rect.height > 0.0f;
}

// Pixel-based CSS transition: crossfade between old and new rendered state.
// Stored per-element region, keyed by element id.
struct PixelTransition {
    std::string element_id;        // DOM element id for matching
    clever::paint::Rect bounds;    // bounding rect in buffer coordinates
    std::vector<uint8_t> from_pixels; // RGBA pixels of the pre-transition state
    int width = 0;
    int height = 0;
    CFTimeInterval start_time = 0; // CACurrentMediaTime() at transition start
    float duration_s = 0;          // transition-duration in seconds
    float delay_s = 0;             // transition-delay in seconds
    int timing_function = 0;       // easing type (0=ease, 1=linear, etc.)
    float bezier_x1 = 0, bezier_y1 = 0, bezier_x2 = 1, bezier_y2 = 1;
    int steps_count = 1;
};

@class RenderView;

@protocol RenderViewDelegate <NSObject>
@optional
- (void)renderView:(RenderView*)view didClickLink:(NSString*)href;
- (void)renderView:(RenderView*)view didClickLinkInNewTab:(NSString*)href;
- (void)renderView:(RenderView*)view hoveredLink:(NSString*)url;
- (void)renderView:(RenderView*)view didSubmitForm:(const clever::paint::FormData&)formData;
- (void)renderViewGoBack:(RenderView*)view;
- (void)renderViewGoForward:(RenderView*)view;
- (void)renderViewReload:(RenderView*)view;
- (void)renderViewViewSource:(RenderView*)view;
- (void)renderViewSaveScreenshot:(RenderView*)view;
- (void)renderView:(RenderView*)view didToggleDetails:(int)detailsId;
- (void)renderView:(RenderView*)view didSelectOption:(NSString*)optionText
           atIndex:(int)index forSelectNamed:(NSString*)name;
// Dispatches a JS "click" event to the DOM element at the given CSS/page coordinates.
// Returns YES if event.preventDefault() was called by a JS handler.
- (BOOL)renderView:(RenderView*)view didClickElementAtX:(float)x y:(float)y;
// Called when the user finishes editing an inline text input overlay.
// tag is "input" or "textarea", inputId is a stable identifier for the element.
- (void)renderView:(RenderView*)view didFinishEditingInputWithValue:(NSString*)value;
// Called when the user types each character in the overlay (for live "input" events).
- (void)renderView:(RenderView*)view didChangeInputValue:(NSString*)value;
// Called when the mouse moves, for hover state management, in CSS/page coordinates.
- (void)renderView:(RenderView*)view didMoveMouseAtX:(float)x y:(float)y;
// Dispatches a JS keyboard event (keydown/keyup) to the focused DOM element.
// Returns YES if event.preventDefault() was called by a JS handler.
- (BOOL)renderView:(RenderView*)view
    didKeyEvent:(NSString*)eventType
            key:(NSString*)key
           code:(NSString*)code
        keyCode:(int)keyCode
         repeat:(BOOL)isRepeat
        altKey:(BOOL)altKey
       ctrlKey:(BOOL)ctrlKey
       metaKey:(BOOL)metaKey
      shiftKey:(BOOL)shiftKey;
// Dispatches a JS "contextmenu" event to the DOM element at the given CSS/page coordinates.
// Returns YES if event.preventDefault() was called by a JS handler.
- (BOOL)renderView:(RenderView*)view didContextMenuAtX:(float)x y:(float)y;
// Dispatches a JS "dblclick" event to the DOM element at the given CSS/page coordinates.
// Returns YES if event.preventDefault() was called by a JS handler.
- (BOOL)renderView:(RenderView*)view didDoubleClickAtX:(float)x y:(float)y;
// Called when wheel/trackpad scrolling updates the view scroll position.
// deltaX/deltaY are the applied deltas in view points after normalization.
// isMomentum is YES for inertial phase events from trackpad scrolling.
- (void)renderView:(RenderView*)view
      didScrollToX:(CGFloat)scrollX
                 y:(CGFloat)scrollY
            deltaX:(CGFloat)deltaX
            deltaY:(CGFloat)deltaY
        isMomentum:(BOOL)isMomentum;
@end

// RenderView: NSView subclass that displays the software renderer's pixel buffer.
// Supports scrolling and draws the rendered page content.
@interface RenderView : NSView

@property (nonatomic) CGFloat scrollOffset;
@property (nonatomic) CGFloat scrollOffsetX;
@property (nonatomic) CGFloat contentHeight;
@property (nonatomic) CGFloat pageScale;
@property (nonatomic, weak) id<RenderViewDelegate> delegate;

// CSS overscroll-behavior for the viewport (from html/body element):
// 0=auto (default, allows bounce), 1=contain, 2=none (both clamp scroll)
@property (nonatomic) int overscrollBehaviorX;
@property (nonatomic) int overscrollBehaviorY;

// CSS scroll-behavior for the viewport (from html/body element):
// NO=auto (instant jump), YES=smooth (animated ease-out scroll)
@property (nonatomic) BOOL scrollBehaviorSmooth;

- (void)updateWithRenderer:(const clever::paint::SoftwareRenderer*)renderer;
- (void)setLayoutRoot:(clever::layout::LayoutNode*)layoutRoot;
- (void)updateLinks:(const std::vector<clever::paint::LinkRegion>&)links;
- (void)updateCursorRegions:(const std::vector<clever::paint::CursorRegion>&)regions;
- (void)updateTextRegions:(const std::vector<clever::paint::PaintCommand>&)commands;
- (void)updateSelectionColors:(uint32_t)color bg:(uint32_t)bgColor;
- (void)updateStickyElements:(std::vector<StickyElementInfo>)elements;
- (void)updateFixedElements:(std::vector<FixedElementInfo>)elements;
- (void)updateFormSubmitRegions:(const std::vector<clever::paint::FormSubmitRegion>&)regions;
- (void)updateFormData:(const std::vector<clever::paint::FormData>&)forms;
- (void)updateDetailsToggleRegions:(const std::vector<clever::paint::DetailsToggleRegion>&)regions;
- (void)updateSelectClickRegions:(const std::vector<clever::paint::SelectClickRegion>&)regions;
- (void)clearContent;
- (NSString*)selectedText;
- (CGFloat)rendererScale;
- (void)syncGeometryToCurrentRendererBuffer;
- (void)setRenderedDocumentOriginX:(CGFloat)documentX;
- (CGFloat)renderedDocumentOriginX;
- (CGFloat)viewOffsetForDocumentX:(CGFloat)documentX;
- (CGFloat)documentXForViewOffset:(CGFloat)viewOffset;
- (CGFloat)viewOffsetForDocumentY:(CGFloat)documentY;
- (CGFloat)documentYForViewOffset:(CGFloat)viewOffset;
- (CGFloat)viewOffsetForRendererY:(CGFloat)rendererY;
- (CGFloat)rendererYForViewOffset:(CGFloat)viewOffset;

// Inline text input overlay — shown over rendered <input>/<textarea> elements.
// Bounds are expected in document logical/page coordinates (CSS pixels).
- (void)showTextInputOverlayWithBounds:(clever::paint::Rect)documentBounds
                                 value:(NSString*)value
                            isPassword:(BOOL)isPassword;
- (void)dismissTextInputOverlay;
- (BOOL)hasTextInputOverlay;
// Recompute the overlay frame from stored document-space bounds and current geometry.
- (void)reflowTextInputOverlay;

// CSS Transition animation: pixel-crossfade between old and new rendered states.
- (void)addPixelTransitions:(std::vector<PixelTransition>)transitions;
- (BOOL)hasActiveTransitions;

// Access rendered pixel buffer for transition snapshotting
- (const std::vector<uint8_t>&)basePixels;
- (int)baseWidth;
- (int)baseHeight;

@end
