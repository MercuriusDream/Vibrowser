#pragma once
#import <Cocoa/Cocoa.h>
#include <clever/paint/display_list.h>
#include <clever/paint/render_pipeline.h>
#include <clever/paint/software_renderer.h>
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
- (CGFloat)viewOffsetForDocumentX:(CGFloat)documentX;
- (CGFloat)documentXForViewOffset:(CGFloat)viewOffset;
- (CGFloat)viewOffsetForDocumentY:(CGFloat)documentY;
- (CGFloat)documentYForViewOffset:(CGFloat)viewOffset;
- (CGFloat)viewOffsetForRendererY:(CGFloat)rendererY;
- (CGFloat)rendererYForViewOffset:(CGFloat)viewOffset;

// Inline text input overlay — shown over rendered <input>/<textarea> elements.
// bufferBounds is in buffer-pixel coordinates (same space as element regions).
- (void)showTextInputOverlayWithBounds:(clever::paint::Rect)bufferBounds
                                 value:(NSString*)value
                            isPassword:(BOOL)isPassword;
- (void)dismissTextInputOverlay;
- (BOOL)hasTextInputOverlay;

// CSS Transition animation: pixel-crossfade between old and new rendered states.
- (void)addPixelTransitions:(std::vector<PixelTransition>)transitions;
- (BOOL)hasActiveTransitions;

// Access rendered pixel buffer for transition snapshotting
- (const std::vector<uint8_t>&)basePixels;
- (int)baseWidth;
- (int)baseHeight;

@end
