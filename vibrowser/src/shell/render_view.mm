#import "render_view.h"
#include <algorithm>
#include <cmath>
#include <QuartzCore/CABase.h>

static constexpr CGFloat kMouseWheelLineStep = 16.0;
static constexpr CGFloat kSmoothScrollFrameInterval = 1.0 / 60.0;
static constexpr CGFloat kScrollDeltaEpsilon = 0.01;

// Cubic bezier easing for CSS transitions (local copy to avoid render_pipeline dependency)
static float _cubic_bezier(float p1x, float p1y, float p2x, float p2y, float t) {
    auto bx = [p1x, p2x](float u) -> float {
        float inv = 1.0f - u;
        return 3.0f * inv * inv * u * p1x + 3.0f * inv * u * u * p2x + u * u * u;
    };
    auto bx_d = [p1x, p2x](float u) -> float {
        float inv = 1.0f - u;
        return 3.0f * inv * inv * p1x + 6.0f * inv * u * (p2x - p1x) + 3.0f * u * u * (1.0f - p2x);
    };
    float u = t;
    for (int i = 0; i < 8; i++) {
        float x = bx(u) - t;
        float dx = bx_d(u);
        if (std::fabs(dx) < 1e-6f) break;
        u -= x / dx;
        u = std::max(0.0f, std::min(1.0f, u));
    }
    float inv = 1.0f - u;
    return 3.0f * inv * inv * u * p1y + 3.0f * inv * u * u * p2y + u * u * u;
}

static float _apply_easing(int tf, float t, float bx1, float by1, float bx2, float by2, int steps) {
    t = std::max(0.0f, std::min(1.0f, t));
    switch (tf) {
        case 1: return t; // linear
        case 2: return _cubic_bezier(0.42f, 0.0f, 1.0f, 1.0f, t); // ease-in
        case 3: return _cubic_bezier(0.0f, 0.0f, 0.58f, 1.0f, t); // ease-out
        case 4: return _cubic_bezier(0.42f, 0.0f, 0.58f, 1.0f, t); // ease-in-out
        case 5: return _cubic_bezier(bx1, by1, bx2, by2, t); // custom
        case 6: return (steps > 0) ? std::floor(t * steps) / steps : t;
        case 7: return (steps > 0) ? std::ceil(t * steps) / steps : t;
        default: return _cubic_bezier(0.25f, 0.1f, 0.25f, 1.0f, t); // ease
    }
}

struct TextRegion {
    clever::paint::Rect bounds;
    std::string text;
    float font_size;
};

@implementation RenderView {
    CGImageRef _cgImage;
    int _imageWidth;
    int _imageHeight;
    CGFloat _backingScale; // Retina scale factor (2.0 on HiDPI, 1.0 otherwise)
    std::vector<clever::paint::LinkRegion> _links;
    std::vector<clever::paint::CursorRegion> _cursorRegions;
    std::vector<TextRegion> _textRegions;

    // Selection state
    BOOL _selecting;
    NSPoint _selectionStart; // page coordinates
    NSPoint _selectionEnd;   // page coordinates
    BOOL _hasSelection;

    // Scrollbar drag state
    BOOL _draggingScrollbar;
    CGFloat _scrollbarDragStartY;
    CGFloat _scrollbarDragStartOffset;
    CGFloat _scrollX;
    CGFloat _scrollY;

    // Smooth scroll animation state
    CGFloat _targetScrollOffset;
    BOOL _isAnimatingScroll;
    BOOL _scrollAnimationIsMomentum;
    NSTimer* _scrollAnimationTimer;
    CFAbsoluteTime _scrollAnimationStartTime;
    CGFloat _scrollAnimationStartOffset;
    CGFloat _scrollAnimationDuration; // seconds

    // CSS ::selection custom colors
    uint32_t _selectionColor;   // ARGB, 0 = use default
    uint32_t _selectionBgColor; // ARGB, 0 = use default

    // Sticky element overlays for position:sticky
    std::vector<StickyElementInfo> _stickyElements;
    std::vector<CGImageRef> _stickyImages; // pre-created CGImages for each sticky element

    // Form submission data
    std::vector<clever::paint::FormSubmitRegion> _formSubmitRegions;
    std::vector<clever::paint::FormData> _formData;

    // Details toggle regions
    std::vector<clever::paint::DetailsToggleRegion> _detailsToggleRegions;

    // Select click regions (dropdown <select> elements)
    std::vector<clever::paint::SelectClickRegion> _selectClickRegions;
    NSString* _pendingSelectName; // name of the <select> element during menu popup

    // Inline text input overlay
    NSTextField* _overlayTextField;
    NSSecureTextField* _overlaySecureField; // for password inputs
    BOOL _overlayIsPassword;
    clever::paint::Rect _overlayBufferBounds; // position in buffer-pixel space

    // CSS Transition animation: pixel-crossfade compositor
    std::vector<PixelTransition> _activeTransitions;
    NSTimer* _transitionTimer;
    // Copy of the full rendered buffer for compositing transitions
    std::vector<uint8_t> _basePixels;
    int _baseWidth;
    int _baseHeight;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _scrollOffset = 0;
        _contentHeight = 0;
        _pageScale = 1.0;
        _backingScale = 1.0;
        _cgImage = NULL;
        _imageWidth = 0;
        _imageHeight = 0;
        _scrollX = 0;
        _scrollY = 0;
        _targetScrollOffset = 0;
        _isAnimatingScroll = NO;
        _scrollAnimationIsMomentum = NO;
        _scrollAnimationTimer = nil;
        _scrollAnimationDuration = 0.3; // 300ms smooth scroll

        // Set up a tracking area so we receive mouseMoved: events and
        // the system re-evaluates cursor rects as the mouse moves.
        NSTrackingArea* trackingArea = [[NSTrackingArea alloc]
            initWithRect:NSZeroRect
                 options:(NSTrackingMouseMoved |
                          NSTrackingActiveInActiveApp |
                          NSTrackingInVisibleRect |
                          NSTrackingCursorUpdate)
                   owner:self
                userInfo:nil];
        [self addTrackingArea:trackingArea];
    }
    return self;
}

- (void)dealloc {
    [self dismissTextInputOverlay];
    [_scrollAnimationTimer invalidate];
    _scrollAnimationTimer = nil;
    [_transitionTimer invalidate];
    _transitionTimer = nil;
    if (_cgImage) CGImageRelease(_cgImage);
    for (auto img : _stickyImages) {
        if (img) CGImageRelease(img);
    }
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    // The tracking area created in initWithFrame uses NSTrackingInVisibleRect,
    // so it automatically adjusts — no need to remove/re-add.
}

- (BOOL)isFlipped {
    return YES; // Use top-left origin like web content
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)updateWithRenderer:(const clever::paint::SoftwareRenderer*)renderer {
    if (!renderer) {
        [self clearContent];
        return;
    }

    _imageWidth = renderer->width();
    _imageHeight = renderer->height();
    // Infer renderer-to-view scale from image width. The layout/render pipeline
    // uses logical CSS pixels for viewport sizing; any higher raster density
    // should be reflected as an image/view ratio here.
    CGFloat viewWidth = std::max<CGFloat>(1.0, self.bounds.size.width);
    CGFloat inferredScale = static_cast<CGFloat>(_imageWidth) / viewWidth;
    if (!std::isfinite(inferredScale) || inferredScale < 1.0) {
        inferredScale = 1.0;
    }
    _backingScale = inferredScale;
    _contentHeight = _imageHeight / _backingScale; // logical points

    // Convert RGBA pixel buffer to CGImage.
    // Our pixel buffer is top-left origin (row 0 = top of page).
    // We store the CGImageRef and draw it directly in drawRect with a
    // flip transform, bypassing NSImage's flipped-view handling.
    const auto& pixels = renderer->pixels();

    // Keep a copy of the rendered pixels for transition compositing
    _basePixels = pixels;
    _baseWidth = _imageWidth;
    _baseHeight = _imageHeight;

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        const_cast<uint8_t*>(pixels.data()),
        _imageWidth, _imageHeight,
        8, _imageWidth * 4,
        colorSpace,
        static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | kCGBitmapByteOrder32Big
    );

    if (ctx) {
        if (_cgImage) CGImageRelease(_cgImage);
        _cgImage = CGBitmapContextCreateImage(ctx);
        CGContextRelease(ctx);
    }
    CGColorSpaceRelease(colorSpace);

    [self setNeedsDisplay:YES];
}

- (CGFloat)viewOffsetForRendererY:(CGFloat)rendererY {
    return (rendererY / _backingScale) * _pageScale;
}

- (CGFloat)rendererYForViewOffset:(CGFloat)viewOffset {
    return (viewOffset / _pageScale) * _backingScale;
}

- (void)updateLinks:(const std::vector<clever::paint::LinkRegion>&)links {
    _links = links;
    // Refresh cursor rects so the hand cursor appears over newly-loaded links.
    [self.window invalidateCursorRectsForView:self];
}

- (void)updateCursorRegions:(const std::vector<clever::paint::CursorRegion>&)regions {
    _cursorRegions = regions;
    [self.window invalidateCursorRectsForView:self];
}

- (void)updateTextRegions:(const std::vector<clever::paint::PaintCommand>&)commands {
    _textRegions.clear();
    for (auto& cmd : commands) {
        if (cmd.type == clever::paint::PaintCommand::DrawText && !cmd.text.empty()) {
            _textRegions.push_back({cmd.bounds, cmd.text, cmd.font_size});
        }
    }
}

- (void)updateSelectionColors:(uint32_t)color bg:(uint32_t)bgColor {
    _selectionColor = color;
    _selectionBgColor = bgColor;
}

- (void)updateFormSubmitRegions:(const std::vector<clever::paint::FormSubmitRegion>&)regions {
    _formSubmitRegions = regions;
}

- (void)updateFormData:(const std::vector<clever::paint::FormData>&)forms {
    _formData = forms;
}

- (void)updateDetailsToggleRegions:(const std::vector<clever::paint::DetailsToggleRegion>&)regions {
    _detailsToggleRegions = regions;
}

- (void)updateSelectClickRegions:(const std::vector<clever::paint::SelectClickRegion>&)regions {
    _selectClickRegions = regions;
}

- (void)selectMenuItemClicked:(NSMenuItem*)sender {
    int index = static_cast<int>(sender.tag);
    NSString* optionText = sender.title;
    if ([_delegate respondsToSelector:@selector(renderView:didSelectOption:atIndex:forSelectNamed:)]) {
        [_delegate renderView:self didSelectOption:optionText atIndex:index forSelectNamed:_pendingSelectName];
    }
}

- (void)updateStickyElements:(std::vector<StickyElementInfo>)elements {
    // Release old CGImages
    for (auto img : _stickyImages) {
        if (img) CGImageRelease(img);
    }
    _stickyImages.clear();
    _stickyElements = std::move(elements);

    // Pre-create CGImages for each sticky element from their pixel data
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    for (auto& elem : _stickyElements) {
        if (elem.pixels.empty() || elem.pixel_width <= 0 || elem.pixel_height <= 0) {
            _stickyImages.push_back(nullptr);
            continue;
        }
        CGContextRef ctx = CGBitmapContextCreate(
            elem.pixels.data(),
            elem.pixel_width, elem.pixel_height,
            8, elem.pixel_width * 4,
            colorSpace,
            static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | kCGBitmapByteOrder32Big
        );
        if (ctx) {
            _stickyImages.push_back(CGBitmapContextCreateImage(ctx));
            CGContextRelease(ctx);
        } else {
            _stickyImages.push_back(nullptr);
        }
    }
    CGColorSpaceRelease(colorSpace);
}

- (void)clearContent {
    [self stopSmoothScrollAnimation];
    if (_cgImage) { CGImageRelease(_cgImage); _cgImage = NULL; }
    _imageWidth = 0;
    _imageHeight = 0;
    _contentHeight = 0;
    _scrollOffset = 0;
    _scrollX = 0;
    _scrollY = 0;
    _targetScrollOffset = 0;
    _links.clear();
    _cursorRegions.clear();
    _textRegions.clear();
    for (auto img : _stickyImages) {
        if (img) CGImageRelease(img);
    }
    _stickyImages.clear();
    _stickyElements.clear();
    _formSubmitRegions.clear();
    _formData.clear();
    _detailsToggleRegions.clear();
    _hasSelection = NO;
    _selecting = NO;
    [self dismissTextInputOverlay];
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    // Draw white background
    [[NSColor whiteColor] setFill];
    NSRectFill(dirtyRect);

    if (!_cgImage) {
        // Draw placeholder text
        NSDictionary* attrs = @{
            NSFontAttributeName: [NSFont systemFontOfSize:14],
            NSForegroundColorAttributeName: [NSColor secondaryLabelColor]
        };
        NSString* text = @"Enter a URL or type HTML to render";
        NSSize textSize = [text sizeWithAttributes:attrs];
        NSPoint point = NSMakePoint(
            (self.bounds.size.width - textSize.width) / 2,
            (self.bounds.size.height - textSize.height) / 2
        );
        [text drawAtPoint:point withAttributes:attrs];
        return;
    }

    // Draw the rendered page image using CGContextDrawImage.
    // Our pixel buffer is top-left origin (row 0 = top of page).
    // CGImage treats row 0 as bottom.  In a flipped NSView (isFlipped=YES)
    // the CTM already flips y.  CGContextDrawImage does NOT auto-adjust for
    // flipped views (unlike NSImage), so we apply a local flip to get the
    // image right-side-up.
    // The image may be rendered at Retina resolution (2x pixels), so we
    // divide by _backingScale to get the correct logical point dimensions.
    CGFloat img_w = (_imageWidth / _backingScale) * _pageScale;
    CGFloat img_h = (_imageHeight / _backingScale) * _pageScale;
    CGFloat img_y = -_scrollOffset;

    CGContextRef cgctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(cgctx);
    // Clip to viewport bounds to prevent horizontal overflow
    CGContextClipToRect(cgctx, CGRectMake(0, 0, self.bounds.size.width, self.bounds.size.height));
    // Flip vertically within the destination rect
    CGContextTranslateCTM(cgctx, 0, img_y + img_h);
    CGContextScaleCTM(cgctx, 1.0, -1.0);
    CGContextDrawImage(cgctx, CGRectMake(0, 0, img_w, img_h), _cgImage);
    CGContextRestoreGState(cgctx);

    // Draw position:sticky overlays.
    // For each sticky element, determine if it should be "stuck" at its
    // CSS `top` offset based on the current scroll position. If so, draw
    // its pre-rendered pixels at the stuck position on top of the main image.
    // All coordinates here are in device pixels (renderer coords) that we
    // convert to logical points via _backingScale and then scale by _pageScale.
    for (size_t i = 0; i < _stickyElements.size(); i++) {
        if (i >= _stickyImages.size() || !_stickyImages[i]) continue;
        auto& elem = _stickyElements[i];

        // Convert scroll offset from logical view points to renderer pixel coords
        CGFloat scrollPx = _scrollOffset / _pageScale * _backingScale;

        // The element's normal position in the page (renderer pixel coords)
        float normal_y = elem.abs_y;
        // The threshold: element starts sticking when its top edge scrolls
        // past the viewport top + top_offset
        float stick_threshold = elem.top_offset * _backingScale;

        // Is the element scrolled past its stick point?
        // The element's Y relative to viewport = normal_y - scrollPx
        // It should stick when normal_y - scrollPx < stick_threshold
        // i.e., when scrollPx > normal_y - stick_threshold
        bool should_stick = (scrollPx > normal_y - stick_threshold);

        // Also check container bounds: the sticky element should not stick
        // past the bottom of its container. When the container's bottom edge
        // minus the element's height reaches the stick threshold, the element
        // should start scrolling away with the container.
        float max_stick_y = elem.container_bottom - elem.height;
        bool past_container = (scrollPx + stick_threshold > max_stick_y);

        if (!should_stick) continue; // Element is in normal flow position, already drawn in main image

        // Calculate the Y position to draw the sticky element (in renderer pixel coords)
        float draw_y_px;
        if (past_container) {
            // Element is pushed up by the container bottom edge
            draw_y_px = max_stick_y;
        } else {
            // Element is stuck at the top_offset from viewport top
            draw_y_px = scrollPx + stick_threshold;
        }

        // Convert from renderer pixels to logical view points
        CGFloat draw_x = (elem.pixel_x / _backingScale) * _pageScale;
        CGFloat draw_y = (draw_y_px / _backingScale) * _pageScale - _scrollOffset;
        CGFloat draw_w = (elem.pixel_width / _backingScale) * _pageScale;
        CGFloat draw_h = (elem.pixel_height / _backingScale) * _pageScale;

        // Draw the sticky element overlay with the same flip transform as the main image
        CGContextSaveGState(cgctx);
        CGContextTranslateCTM(cgctx, draw_x, draw_y + draw_h);
        CGContextScaleCTM(cgctx, 1.0, -1.0);
        CGContextDrawImage(cgctx, CGRectMake(0, 0, draw_w, draw_h), _stickyImages[i]);
        CGContextRestoreGState(cgctx);
    }

    // Draw scrollbar (macOS-style overlay)
    CGFloat totalHeight = _contentHeight * _pageScale;
    CGFloat viewportHeight = self.bounds.size.height;
    if (totalHeight > viewportHeight) {
        CGFloat scrollbarWidth = 8.0;
        CGFloat scrollbarMargin = 2.0;
        CGFloat scrollbarX = self.bounds.size.width - scrollbarWidth - scrollbarMargin;
        CGFloat trackHeight = viewportHeight - 2 * scrollbarMargin;

        // Calculate thumb size and position
        CGFloat thumbRatio = viewportHeight / totalHeight;
        CGFloat thumbHeight = std::max(30.0, trackHeight * thumbRatio);
        CGFloat scrollRange = totalHeight - viewportHeight;
        CGFloat thumbOffset = (scrollRange > 0) ? (_scrollOffset / scrollRange) * (trackHeight - thumbHeight) : 0;

        // Draw track (subtle)
        NSRect trackRect = NSMakeRect(scrollbarX, scrollbarMargin, scrollbarWidth, trackHeight);
        [[NSColor colorWithCalibratedWhite:0.0 alpha:0.05] setFill];
        NSBezierPath* trackPath = [NSBezierPath bezierPathWithRoundedRect:trackRect xRadius:4 yRadius:4];
        [trackPath fill];

        // Draw thumb
        NSRect thumbRect = NSMakeRect(scrollbarX, scrollbarMargin + thumbOffset, scrollbarWidth, thumbHeight);
        [[NSColor colorWithCalibratedWhite:0.0 alpha:0.4] setFill];
        NSBezierPath* thumbPath = [NSBezierPath bezierPathWithRoundedRect:thumbRect xRadius:4 yRadius:4];
        [thumbPath fill];
    }

    // Draw selection highlight
    if (_hasSelection || _selecting) {
        if (_selectionBgColor != 0) {
            // Use CSS ::selection background-color
            CGFloat sa = (CGFloat)((_selectionBgColor >> 24) & 0xFF) / 255.0;
            CGFloat sr = (CGFloat)((_selectionBgColor >> 16) & 0xFF) / 255.0;
            CGFloat sg = (CGFloat)((_selectionBgColor >> 8) & 0xFF) / 255.0;
            CGFloat sb = (CGFloat)(_selectionBgColor & 0xFF) / 255.0;
            [[NSColor colorWithCalibratedRed:sr green:sg blue:sb alpha:sa] setFill];
        } else {
            [[NSColor colorWithCalibratedRed:0.2 green:0.5 blue:1.0 alpha:0.3] setFill];
        }
        float sx = std::min((float)_selectionStart.x, (float)_selectionEnd.x);
        float sy = std::min((float)_selectionStart.y, (float)_selectionEnd.y);
        float ex = std::max((float)_selectionStart.x, (float)_selectionEnd.x);
        float ey = std::max((float)_selectionStart.y, (float)_selectionEnd.y);

        for (auto& region : _textRegions) {
            auto& b = region.bounds;
            // Convert from pixel coords to logical coords
            float bx = b.x / _backingScale;
            float by = b.y / _backingScale;
            float bw = b.width / _backingScale;
            float bh = b.height / _backingScale;
            // Check if text region overlaps with selection rect
            if (bx + bw >= sx && bx <= ex &&
                by + bh >= sy && by <= ey) {
                NSRect highlight = NSMakeRect(
                    bx * _pageScale,
                    by * _pageScale - _scrollOffset,
                    bw * _pageScale,
                    bh * _pageScale);
                NSRectFillUsingOperation(highlight, NSCompositingOperationSourceOver);
            }
        }
    }
}

- (void)scrollWheel:(NSEvent*)event {
    // Dismiss text input overlay when scrolling — it would be mispositioned
    if (_overlayTextField || _overlaySecureField) {
        NSTextField* field = _overlayIsPassword ? _overlaySecureField : _overlayTextField;
        NSString* val = [field stringValue];
        if ([_delegate respondsToSelector:@selector(renderView:didFinishEditingInputWithValue:)]) {
            [_delegate renderView:self didFinishEditingInputWithValue:val];
        }
        [self dismissTextInputOverlay];
    }

    // Keep internal tracking in sync when external code updates scrollOffset.
    _scrollY = _scrollOffset;

    CGFloat rawDeltaX = event.scrollingDeltaX;
    CGFloat rawDeltaY = event.scrollingDeltaY;
    if (!event.hasPreciseScrollingDeltas) {
        // Mouse wheel deltas are line-based; convert to point-space.
        rawDeltaX *= kMouseWheelLineStep;
        rawDeltaY *= kMouseWheelLineStep;
    }

    // Normalize to content-space scroll amounts:
    // positive => scroll right/down, negative => scroll left/up.
    CGFloat scrollAmountX = -rawDeltaX;
    CGFloat scrollAmountY = -rawDeltaY;
    BOOL isMomentum = (event.momentumPhase != NSEventPhaseNone);

    // CSS overscroll-behavior: contain (1) or none (2) on viewport means clamp
    // and consume boundary-pushing wheel input to prevent scroll chaining/bounce.
    bool clampOverscrollY = (_overscrollBehaviorY == 1 || _overscrollBehaviorY == 2);

    CGFloat maxScroll = std::max(0.0, _contentHeight * _pageScale - self.bounds.size.height);
    CGFloat appliedDeltaX = 0;
    CGFloat appliedDeltaY = 0;

    // Track horizontal wheel movement for delegate/DOM consumers.
    // Horizontal page translation is not currently rendered in this view.
    CGFloat prevScrollX = _scrollX;
    _scrollX += scrollAmountX;
    _scrollX = std::max(0.0, _scrollX);
    appliedDeltaX = _scrollX - prevScrollX;

    // Overscroll boundary check uses current animation target while animating.
    bool consumeVerticalEvent = false;
    if (clampOverscrollY) {
        CGFloat checkOffset = _isAnimatingScroll ? _targetScrollOffset : _scrollOffset;
        bool atTop = (checkOffset <= 0.0);
        bool atBottom = (checkOffset >= maxScroll);
        // scrollAmountY < 0 => up, scrollAmountY > 0 => down.
        consumeVerticalEvent = (atTop && scrollAmountY < 0) ||
                               (atBottom && scrollAmountY > 0 && maxScroll > 0);
    }

    if (!consumeVerticalEvent && _scrollBehaviorSmooth && std::abs(scrollAmountY) > kScrollDeltaEpsilon) {
        // Smooth scroll: animate toward the target offset using ease-out
        if (_isAnimatingScroll) {
            _targetScrollOffset += scrollAmountY;
        } else {
            _targetScrollOffset = _scrollOffset + scrollAmountY;
        }
        _targetScrollOffset = std::max(0.0, std::min(_targetScrollOffset, maxScroll));

        // (Re)start the animation from the current position.
        _scrollAnimationStartOffset = _scrollOffset;
        _scrollAnimationStartTime = CFAbsoluteTimeGetCurrent();
        _scrollAnimationIsMomentum = isMomentum;
        if (!_isAnimatingScroll) {
            _isAnimatingScroll = YES;
            _scrollAnimationTimer = [NSTimer scheduledTimerWithTimeInterval:kSmoothScrollFrameInterval
                                                                    target:self
                                                                  selector:@selector(smoothScrollTick:)
                                                                  userInfo:nil
                                                                   repeats:YES];
            // Ensure timer fires during event tracking (e.g. during scroll gestures)
            [[NSRunLoop currentRunLoop] addTimer:_scrollAnimationTimer
                                         forMode:NSEventTrackingRunLoopMode];
        }
    } else if (!consumeVerticalEvent && std::abs(scrollAmountY) > kScrollDeltaEpsilon) {
        // Instant scroll (default behavior)
        CGFloat prevScrollY = _scrollOffset;
        _scrollOffset += scrollAmountY;
        _scrollOffset = std::max(0.0, std::min(_scrollOffset, maxScroll));
        _scrollY = _scrollOffset;
        appliedDeltaY = _scrollY - prevScrollY;
        [self setNeedsDisplay:YES];
    } else {
        _scrollY = _scrollOffset;
    }

    // Notify delegate so the embedding controller can react/re-render.
    if ((std::abs(appliedDeltaX) > kScrollDeltaEpsilon || std::abs(appliedDeltaY) > kScrollDeltaEpsilon) &&
        [_delegate respondsToSelector:@selector(renderView:didScrollToX:y:deltaX:deltaY:isMomentum:)]) {
        [_delegate renderView:self
                 didScrollToX:_scrollX
                            y:_scrollY
                       deltaX:appliedDeltaX
                       deltaY:appliedDeltaY
                   isMomentum:isMomentum];
    }

    // Only update cursor rects at end of scroll gesture (phase == ended or none)
    if (event.phase == NSEventPhaseEnded || event.phase == NSEventPhaseNone ||
        event.momentumPhase == NSEventPhaseEnded) {
        [self.window invalidateCursorRectsForView:self];
    }
}

- (void)smoothScrollTick:(NSTimer*)timer {
    (void)timer;
    CGFloat prevScrollY = _scrollOffset;
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    CFAbsoluteTime elapsed = now - _scrollAnimationStartTime;
    CGFloat progress = elapsed / _scrollAnimationDuration;

    if (progress >= 1.0) {
        // Animation complete — snap to target
        _scrollOffset = _targetScrollOffset;
        [self stopSmoothScrollAnimation];
    } else {
        // Cubic ease-out: t = 1 - (1 - progress)^3
        CGFloat t = 1.0 - pow(1.0 - progress, 3.0);
        _scrollOffset = _scrollAnimationStartOffset + (_targetScrollOffset - _scrollAnimationStartOffset) * t;
    }

    // Check if close enough to target (within 0.5 pixels)
    if (std::abs(_scrollOffset - _targetScrollOffset) < 0.5) {
        _scrollOffset = _targetScrollOffset;
        [self stopSmoothScrollAnimation];
    }

    _scrollY = _scrollOffset;

    CGFloat appliedDeltaY = _scrollOffset - prevScrollY;
    if (std::abs(appliedDeltaY) > kScrollDeltaEpsilon &&
        [_delegate respondsToSelector:@selector(renderView:didScrollToX:y:deltaX:deltaY:isMomentum:)]) {
        [_delegate renderView:self
                 didScrollToX:_scrollX
                            y:_scrollY
                       deltaX:0.0
                       deltaY:appliedDeltaY
                   isMomentum:_scrollAnimationIsMomentum];
    }

    [self setNeedsDisplay:YES];
}

- (void)stopSmoothScrollAnimation {
    if (_scrollAnimationTimer) {
        [_scrollAnimationTimer invalidate];
        _scrollAnimationTimer = nil;
    }
    _isAnimatingScroll = NO;
    _scrollAnimationIsMomentum = NO;
    [self.window invalidateCursorRectsForView:self];
}

- (void)magnifyWithEvent:(NSEvent*)event {
    _pageScale += event.magnification;
    _pageScale = std::max(0.25, std::min(4.0, _pageScale));
    [self setNeedsDisplay:YES];
    [self.window invalidateCursorRectsForView:self];
}

// Returns YES if the scrollbar is visible and fills out geometry values.
- (BOOL)scrollbarGeometryWithTrackRect:(NSRect*)outTrack
                             thumbRect:(NSRect*)outThumb {
    CGFloat totalHeight = _contentHeight * _pageScale;
    CGFloat viewportHeight = self.bounds.size.height;
    if (totalHeight <= viewportHeight) return NO;

    CGFloat scrollbarWidth = 8.0;
    CGFloat scrollbarMargin = 2.0;
    CGFloat scrollbarX = self.bounds.size.width - scrollbarWidth - scrollbarMargin;
    CGFloat trackHeight = viewportHeight - 2 * scrollbarMargin;

    CGFloat thumbRatio = viewportHeight / totalHeight;
    CGFloat thumbHeight = std::max(30.0, trackHeight * thumbRatio);
    CGFloat scrollRange = totalHeight - viewportHeight;
    CGFloat thumbOffset = (scrollRange > 0) ? (_scrollOffset / scrollRange) * (trackHeight - thumbHeight) : 0;

    if (outTrack) *outTrack = NSMakeRect(scrollbarX, scrollbarMargin, scrollbarWidth, trackHeight);
    if (outThumb) *outThumb = NSMakeRect(scrollbarX, scrollbarMargin + thumbOffset, scrollbarWidth, thumbHeight);
    return YES;
}

- (void)mouseDown:(NSEvent*)event {
    // Explicitly claim first responder so keyboard scrolling works after clicking
    // in the render view. This is the only path that makes RenderView first responder.
    [self.window makeFirstResponder:self];

    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];

    // Check if click is on the scrollbar area
    NSRect trackRect, thumbRect;
    if ([self scrollbarGeometryWithTrackRect:&trackRect thumbRect:&thumbRect]) {
        if (NSPointInRect(loc, trackRect)) {
            if (NSPointInRect(loc, thumbRect)) {
                // Start dragging the thumb
                _draggingScrollbar = YES;
                _scrollbarDragStartY = loc.y;
                _scrollbarDragStartOffset = _scrollOffset;
            } else {
                // Click on track — jump scroll position
                CGFloat totalHeight = _contentHeight * _pageScale;
                CGFloat viewportHeight = self.bounds.size.height;
                CGFloat scrollRange = totalHeight - viewportHeight;
                CGFloat clickRatio = (loc.y - trackRect.origin.y) / trackRect.size.height;
                _scrollOffset = clickRatio * scrollRange;
                _scrollOffset = std::max(0.0, std::min(_scrollOffset, scrollRange));

                // Also start dragging from this new position
                _draggingScrollbar = YES;
                _scrollbarDragStartY = loc.y;
                _scrollbarDragStartOffset = _scrollOffset;
            }
            [self setNeedsDisplay:YES];
            [self.window invalidateCursorRectsForView:self];
            return;
        }
    }

    float px = static_cast<float>(loc.x / _pageScale);
    float py = static_cast<float>((loc.y + _scrollOffset) / _pageScale);

    _selectionStart = NSMakePoint(px, py);
    _selectionEnd = NSMakePoint(px, py);
    _selecting = YES;
    _hasSelection = NO;
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent*)event {
    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];

    if (_draggingScrollbar) {
        CGFloat totalHeight = _contentHeight * _pageScale;
        CGFloat viewportHeight = self.bounds.size.height;
        CGFloat scrollRange = totalHeight - viewportHeight;
        CGFloat trackHeight = viewportHeight - 4.0; // 2*margin
        CGFloat thumbRatio = viewportHeight / totalHeight;
        CGFloat thumbHeight = std::max(30.0, trackHeight * thumbRatio);
        CGFloat usableTrack = trackHeight - thumbHeight;

        if (usableTrack > 0) {
            CGFloat deltaY = loc.y - _scrollbarDragStartY;
            _scrollOffset = _scrollbarDragStartOffset + (deltaY / usableTrack) * scrollRange;
            _scrollOffset = std::max(0.0, std::min(_scrollOffset, scrollRange));
        }
        [self setNeedsDisplay:YES];
        [self.window invalidateCursorRectsForView:self];
        return;
    }

    if (!_selecting) return;

    float px = static_cast<float>(loc.x / _pageScale);
    float py = static_cast<float>((loc.y + _scrollOffset) / _pageScale);

    _selectionEnd = NSMakePoint(px, py);
    _hasSelection = YES;
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent*)event {
    if (_draggingScrollbar) {
        _draggingScrollbar = NO;
        return;
    }

    _selecting = NO;

    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    float px = static_cast<float>(loc.x / _pageScale);
    float py = static_cast<float>((loc.y + _scrollOffset) / _pageScale);

    _selectionEnd = NSMakePoint(px, py);

    // If it was just a click (not a drag), try link navigation
    float dx = (float)(_selectionEnd.x - _selectionStart.x);
    float dy = (float)(_selectionEnd.y - _selectionStart.y);
    if (std::abs(dx) < 3 && std::abs(dy) < 3) {
        _hasSelection = NO;
        // Hit-test against link regions (convert pixel coords to logical coords)
        float px_scaled = px * _backingScale;
        float py_scaled = py * _backingScale;

        // Dispatch JS "click" event before handling default behavior.
        // If a JS handler calls preventDefault(), skip all default actions
        // (link navigation, form submission, details toggle, etc.).
        BOOL defaultPrevented = NO;
        if ([_delegate respondsToSelector:@selector(renderView:didClickElementAtX:y:)]) {
            defaultPrevented = [_delegate renderView:self
                                 didClickElementAtX:px_scaled
                                                  y:py_scaled];
        }

        // On double-click, dispatch dblclick event after the click
        if (event.clickCount >= 2) {
            if ([_delegate respondsToSelector:@selector(renderView:didDoubleClickAtX:y:)]) {
                [_delegate renderView:self didDoubleClickAtX:px_scaled y:py_scaled];
            }
        }

        if (defaultPrevented) {
            [self setNeedsDisplay:YES];
            return;
        }

        // Check form submit regions first (they take priority over links)
        for (auto it = _formSubmitRegions.rbegin(); it != _formSubmitRegions.rend(); ++it) {
            if (it->bounds.contains(px_scaled, py_scaled)) {
                int fi = it->form_index;
                if (fi >= 0 && fi < static_cast<int>(_formData.size())) {
                    if ([_delegate respondsToSelector:@selector(renderView:didSubmitForm:)]) {
                        [_delegate renderView:self didSubmitForm:_formData[fi]];
                    }
                    return;
                }
            }
        }

        // Check select click regions (clicking a <select> opens a popup menu)
        for (auto it = _selectClickRegions.rbegin(); it != _selectClickRegions.rend(); ++it) {
            if (it->bounds.contains(px_scaled, py_scaled)) {
                // Build and show a native NSMenu with the options
                NSMenu* menu = [[NSMenu alloc] initWithTitle:@""];
                for (int i = 0; i < static_cast<int>(it->options.size()); i++) {
                    NSString* title = [NSString stringWithUTF8String:it->options[i].c_str()];
                    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                                 action:@selector(selectMenuItemClicked:)
                                                          keyEquivalent:@""];
                    item.target = self;
                    item.tag = i;
                    if (i == it->selected_index) {
                        item.state = NSControlStateValueOn;
                    }
                    [menu addItem:item];
                }
                // Store the region info for the callback
                _pendingSelectName = [NSString stringWithUTF8String:it->name.c_str()];
                // Show the menu at the click location
                [menu popUpMenuPositioningItem:nil
                                    atLocation:loc
                                        inView:self];
                return;
            }
        }

        // Check details toggle regions (clicking <summary> toggles <details>)
        for (auto it = _detailsToggleRegions.rbegin(); it != _detailsToggleRegions.rend(); ++it) {
            if (it->bounds.contains(px_scaled, py_scaled)) {
                if ([_delegate respondsToSelector:@selector(renderView:didToggleDetails:)]) {
                    [_delegate renderView:self didToggleDetails:it->details_id];
                }
                return;
            }
        }

        for (auto it = _links.rbegin(); it != _links.rend(); ++it) {
            if (it->bounds.contains(px_scaled, py_scaled)) {
                NSString* href = [NSString stringWithUTF8String:it->href.c_str()];
                if (it->target == "_blank") {
                    if ([_delegate respondsToSelector:@selector(renderView:didClickLinkInNewTab:)]) {
                        [_delegate renderView:self didClickLinkInNewTab:href];
                    }
                } else if ([_delegate respondsToSelector:@selector(renderView:didClickLink:)]) {
                    [_delegate renderView:self didClickLink:href];
                }
                return;
            }
        }
    }

    [self setNeedsDisplay:YES];
}

- (void)mouseMoved:(NSEvent*)event {
    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    float px = static_cast<float>(loc.x / _pageScale);
    float py = static_cast<float>((loc.y + _scrollOffset) / _pageScale);

    // Hit-test against link regions (convert logical to pixel coords)
    float px_scaled = px * _backingScale;
    float py_scaled = py * _backingScale;
    for (auto it = _links.rbegin(); it != _links.rend(); ++it) {
        if (it->bounds.contains(px_scaled, py_scaled)) {
            [[NSCursor pointingHandCursor] set];
            // Notify delegate about hovered link URL
            if ([_delegate respondsToSelector:@selector(renderView:hoveredLink:)]) {
                NSString* href = [NSString stringWithUTF8String:it->href.c_str()];
                [_delegate renderView:self hoveredLink:href];
            }
            return;
        }
    }

    // No link hovered — clear the status bar
    if ([_delegate respondsToSelector:@selector(renderView:hoveredLink:)]) {
        [_delegate renderView:self hoveredLink:nil];
    }

    // Hit-test against CSS cursor regions (last-painted = highest z-order)
    for (auto it = _cursorRegions.rbegin(); it != _cursorRegions.rend(); ++it) {
        if (it->bounds.contains(px_scaled, py_scaled)) {
            switch (it->cursor_type) {
                case 1: [[NSCursor arrowCursor] set]; return;           // default
                case 2: [[NSCursor pointingHandCursor] set]; return;    // pointer
                case 3: [[NSCursor IBeamCursor] set]; return;           // text
                case 4: [[NSCursor openHandCursor] set]; return;        // move
                case 5: [[NSCursor operationNotAllowedCursor] set]; return; // not-allowed
                default: break;
            }
        }
    }
    [[NSCursor arrowCursor] set];

    // Notify delegate for CSS :hover support
    if ([_delegate respondsToSelector:@selector(renderView:didMoveMouseAtX:y:)]) {
        [_delegate renderView:self didMoveMouseAtX:px_scaled y:py_scaled];
    }
}

// Right-click context menu
- (NSMenu*)menuForEvent:(NSEvent*)event {
    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    float px = static_cast<float>(loc.x / _pageScale) * _backingScale;
    float py = static_cast<float>((loc.y + _scrollOffset) / _pageScale) * _backingScale;

    // Dispatch JS "contextmenu" event. If preventDefault() was called, suppress the native menu.
    if ([_delegate respondsToSelector:@selector(renderView:didContextMenuAtX:y:)]) {
        BOOL prevented = [_delegate renderView:self didContextMenuAtX:px y:py];
        if (prevented) return nil;
    }

    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Context"];

    // Check if right-clicking on a link
    NSString* linkHref = nil;
    for (auto it = _links.rbegin(); it != _links.rend(); ++it) {
        if (it->bounds.contains(px, py)) {
            linkHref = [NSString stringWithUTF8String:it->href.c_str()];
            break;
        }
    }

    if (linkHref) {
        NSMenuItem* openLink = [[NSMenuItem alloc] initWithTitle:@"Open Link"
                                                          action:@selector(contextOpenLink:)
                                                   keyEquivalent:@""];
        openLink.representedObject = linkHref;
        openLink.target = self;
        [menu addItem:openLink];

        NSMenuItem* copyLink = [[NSMenuItem alloc] initWithTitle:@"Copy Link URL"
                                                          action:@selector(contextCopyLinkURL:)
                                                   keyEquivalent:@""];
        copyLink.representedObject = linkHref;
        copyLink.target = self;
        [menu addItem:copyLink];

        [menu addItem:[NSMenuItem separatorItem]];
    }

    // Copy selected text (if any)
    NSString* selectedText = [self selectedText];
    if (selectedText.length > 0) {
        NSMenuItem* copyItem = [[NSMenuItem alloc] initWithTitle:@"Copy"
                                                          action:@selector(contextCopy:)
                                                   keyEquivalent:@""];
        copyItem.target = self;
        [menu addItem:copyItem];
        [menu addItem:[NSMenuItem separatorItem]];
    }

    // Navigation
    NSMenuItem* backItem = [[NSMenuItem alloc] initWithTitle:@"Back"
                                                      action:@selector(contextGoBack:)
                                               keyEquivalent:@""];
    backItem.target = self;
    [menu addItem:backItem];

    NSMenuItem* fwdItem = [[NSMenuItem alloc] initWithTitle:@"Forward"
                                                     action:@selector(contextGoForward:)
                                              keyEquivalent:@""];
    fwdItem.target = self;
    [menu addItem:fwdItem];

    NSMenuItem* reloadItem = [[NSMenuItem alloc] initWithTitle:@"Reload"
                                                        action:@selector(contextReload:)
                                                 keyEquivalent:@""];
    reloadItem.target = self;
    [menu addItem:reloadItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* sourceItem = [[NSMenuItem alloc] initWithTitle:@"View Source"
                                                        action:@selector(contextViewSource:)
                                                 keyEquivalent:@""];
    sourceItem.target = self;
    [menu addItem:sourceItem];

    NSMenuItem* screenshotItem = [[NSMenuItem alloc] initWithTitle:@"Save Screenshot"
                                                            action:@selector(contextSaveScreenshot:)
                                                     keyEquivalent:@""];
    screenshotItem.target = self;
    [menu addItem:screenshotItem];

    return menu;
}

- (void)contextOpenLink:(NSMenuItem*)item {
    NSString* href = item.representedObject;
    if ([_delegate respondsToSelector:@selector(renderView:didClickLink:)]) {
        [_delegate renderView:self didClickLink:href];
    }
}

- (void)contextCopyLinkURL:(NSMenuItem*)item {
    NSString* href = item.representedObject;
    NSPasteboard* pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    [pb setString:href forType:NSPasteboardTypeString];
}

- (void)contextCopy:(NSMenuItem*)item {
    (void)item;
    NSString* text = [self selectedText];
    if (text.length > 0) {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb setString:text forType:NSPasteboardTypeString];
    }
}

- (void)contextGoBack:(NSMenuItem*)item {
    (void)item;
    if ([_delegate respondsToSelector:@selector(renderViewGoBack:)]) {
        [_delegate renderViewGoBack:self];
    }
}

- (void)contextGoForward:(NSMenuItem*)item {
    (void)item;
    if ([_delegate respondsToSelector:@selector(renderViewGoForward:)]) {
        [_delegate renderViewGoForward:self];
    }
}

- (void)contextReload:(NSMenuItem*)item {
    (void)item;
    if ([_delegate respondsToSelector:@selector(renderViewReload:)]) {
        [_delegate renderViewReload:self];
    }
}

- (void)contextViewSource:(NSMenuItem*)item {
    (void)item;
    if ([_delegate respondsToSelector:@selector(renderViewViewSource:)]) {
        [_delegate renderViewViewSource:self];
    }
}

- (void)contextSaveScreenshot:(NSMenuItem*)item {
    (void)item;
    if ([_delegate respondsToSelector:@selector(renderViewSaveScreenshot:)]) {
        [_delegate renderViewSaveScreenshot:self];
    }
}

// Helper: convert macOS keyCode to DOM "code" string (physical key identifier)
static NSString* macKeyCodeToCode(unsigned short keyCode) {
    switch (keyCode) {
        case 0: return @"KeyA"; case 1: return @"KeyS"; case 2: return @"KeyD";
        case 3: return @"KeyF"; case 4: return @"KeyH"; case 5: return @"KeyG";
        case 6: return @"KeyZ"; case 7: return @"KeyX"; case 8: return @"KeyC";
        case 9: return @"KeyV"; case 11: return @"KeyB"; case 12: return @"KeyQ";
        case 13: return @"KeyW"; case 14: return @"KeyE"; case 15: return @"KeyR";
        case 16: return @"KeyY"; case 17: return @"KeyT"; case 18: return @"Digit1";
        case 19: return @"Digit2"; case 20: return @"Digit3"; case 21: return @"Digit4";
        case 22: return @"Digit6"; case 23: return @"Digit5"; case 24: return @"Equal";
        case 25: return @"Digit9"; case 26: return @"Digit7"; case 27: return @"Minus";
        case 28: return @"Digit8"; case 29: return @"Digit0"; case 30: return @"BracketRight";
        case 31: return @"KeyO"; case 32: return @"KeyU"; case 33: return @"BracketLeft";
        case 34: return @"KeyI"; case 35: return @"KeyP"; case 36: return @"Enter";
        case 37: return @"KeyL"; case 38: return @"KeyJ"; case 39: return @"Quote";
        case 40: return @"KeyK"; case 41: return @"Semicolon"; case 42: return @"Backslash";
        case 43: return @"Comma"; case 44: return @"Slash"; case 45: return @"KeyN";
        case 46: return @"KeyM"; case 47: return @"Period"; case 48: return @"Tab";
        case 49: return @"Space"; case 50: return @"Backquote";
        case 51: return @"Backspace"; case 53: return @"Escape";
        case 55: return @"MetaLeft"; case 56: return @"ShiftLeft";
        case 57: return @"CapsLock"; case 58: return @"AltLeft";
        case 59: return @"ControlLeft"; case 60: return @"ShiftRight";
        case 61: return @"AltRight"; case 62: return @"ControlRight";
        case 65: return @"NumpadDecimal"; case 67: return @"NumpadMultiply";
        case 69: return @"NumpadAdd"; case 71: return @"NumLock";
        case 75: return @"NumpadDivide"; case 76: return @"NumpadEnter";
        case 78: return @"NumpadSubtract"; case 82: return @"Numpad0";
        case 83: return @"Numpad1"; case 84: return @"Numpad2";
        case 85: return @"Numpad3"; case 86: return @"Numpad4";
        case 87: return @"Numpad5"; case 88: return @"Numpad6";
        case 89: return @"Numpad7"; case 91: return @"Numpad8";
        case 92: return @"Numpad9";
        case 115: return @"Home"; case 116: return @"PageUp";
        case 117: return @"Delete"; case 119: return @"End";
        case 121: return @"PageDown";
        case 123: return @"ArrowLeft"; case 124: return @"ArrowRight";
        case 125: return @"ArrowDown"; case 126: return @"ArrowUp";
        case 96: return @"F5"; case 97: return @"F6"; case 98: return @"F7";
        case 99: return @"F3"; case 100: return @"F8"; case 101: return @"F9";
        case 109: return @"F10"; case 111: return @"F12";
        case 118: return @"F4"; case 120: return @"F2"; case 122: return @"F1";
        default: return @"Unidentified";
    }
}

// Helper: convert macOS keyCode to DOM "key" string (logical key value)
static NSString* macKeyCodeToKeyName(unsigned short keyCode) {
    switch (keyCode) {
        case 36: return @"Enter"; case 48: return @"Tab"; case 49: return @"  ";
        case 51: return @"Backspace"; case 53: return @"Escape";
        case 55: return @"Meta"; case 56: return @"Shift";
        case 57: return @"CapsLock"; case 58: return @"Alt";
        case 59: return @"Control"; case 60: return @"Shift";
        case 61: return @"Alt"; case 62: return @"Control";
        case 115: return @"Home"; case 116: return @"PageUp";
        case 117: return @"Delete"; case 119: return @"End";
        case 121: return @"PageDown";
        case 123: return @"ArrowLeft"; case 124: return @"ArrowRight";
        case 125: return @"ArrowDown"; case 126: return @"ArrowUp";
        case 96: return @"F5"; case 97: return @"F6"; case 98: return @"F7";
        case 99: return @"F3"; case 100: return @"F8"; case 101: return @"F9";
        case 109: return @"F10"; case 111: return @"F12";
        case 118: return @"F4"; case 120: return @"F2"; case 122: return @"F1";
        default: return nil; // nil means use characters string
    }
}

// Helper: get DOM keyCode (legacy) from macOS keyCode
static int macKeyCodeToDOMKeyCode(unsigned short keyCode, NSString* characters) {
    switch (keyCode) {
        case 36: return 13;  // Enter
        case 48: return 9;   // Tab
        case 49: return 32;  // Space
        case 51: return 8;   // Backspace
        case 53: return 27;  // Escape
        case 117: return 46; // Delete
        case 123: return 37; // ArrowLeft
        case 124: return 39; // ArrowRight
        case 125: return 40; // ArrowDown
        case 126: return 38; // ArrowUp
        case 115: return 36; // Home
        case 119: return 35; // End
        case 116: return 33; // PageUp
        case 121: return 34; // PageDown
        case 122: return 112; // F1
        case 120: return 113; // F2
        case 99:  return 114; // F3
        case 118: return 115; // F4
        case 96:  return 116; // F5
        case 97:  return 117; // F6
        case 98:  return 118; // F7
        case 100: return 119; // F8
        case 101: return 120; // F9
        case 109: return 121; // F10
        case 111: return 123; // F12
        default: {
            // For letter/number keys, use the uppercase character code
            if (characters && characters.length > 0) {
                unichar ch = [characters characterAtIndex:0];
                if (ch >= 'a' && ch <= 'z') return ch - 'a' + 'A';
                if (ch >= 'A' && ch <= 'Z') return ch;
                if (ch >= '0' && ch <= '9') return ch;
                return ch;
            }
            return 0;
        }
    }
}

// Helper: dispatch a JS keyboard event via delegate. Returns YES if preventDefault() was called.
- (BOOL)dispatchJSKeyEvent:(NSString*)eventType fromNSEvent:(NSEvent*)event {
    if (![_delegate respondsToSelector:@selector(renderView:didKeyEvent:key:code:keyCode:repeat:altKey:ctrlKey:metaKey:shiftKey:)]) {
        return NO;
    }

    unsigned short kc = event.keyCode;
    NSString* code = macKeyCodeToCode(kc);
    NSString* keyName = macKeyCodeToKeyName(kc);
    NSString* key = keyName;
    if (!key) {
        // Use the characters string for printable keys
        NSString* chars = event.characters;
        key = (chars && chars.length > 0) ? chars : @"Unidentified";
    }
    // Fix: Space bar keyName workaround (was "  " to avoid conflict)
    if (kc == 49) key = @" ";

    int domKeyCode = macKeyCodeToDOMKeyCode(kc, event.charactersIgnoringModifiers);
    BOOL altKey = (event.modifierFlags & NSEventModifierFlagOption) != 0;
    BOOL ctrlKey = (event.modifierFlags & NSEventModifierFlagControl) != 0;
    BOOL metaKey = (event.modifierFlags & NSEventModifierFlagCommand) != 0;
    BOOL shiftKey = (event.modifierFlags & NSEventModifierFlagShift) != 0;

    return [_delegate renderView:self
                     didKeyEvent:eventType
                             key:key
                            code:code
                         keyCode:domKeyCode
                          repeat:event.isARepeat
                          altKey:altKey
                         ctrlKey:ctrlKey
                         metaKey:metaKey
                        shiftKey:shiftKey];
}

- (void)keyDown:(NSEvent*)event {
    // If the overlay text field is active, let it handle keys —
    // but intercept Tab/Enter/Escape for commit/dismiss.
    if (_overlayTextField || _overlaySecureField) {
        unsigned short kc = event.keyCode;
        if (kc == 36 /* Return */ || kc == 48 /* Tab */) {
            // Commit the value and dismiss overlay
            NSTextField* field = _overlayIsPassword ? _overlaySecureField : _overlayTextField;
            NSString* val = [field stringValue];
            if ([_delegate respondsToSelector:@selector(renderView:didFinishEditingInputWithValue:)]) {
                [_delegate renderView:self didFinishEditingInputWithValue:val];
            }
            [self dismissTextInputOverlay];
            return;
        }
        if (kc == 53 /* Escape */) {
            [self dismissTextInputOverlay];
            return;
        }
        // Forward other keys to the text field
        return;
    }

    // Dispatch JS keydown event to the focused element.
    // If a JS handler calls preventDefault(), skip all default browser actions.
    BOOL jsPrevented = [self dispatchJSKeyEvent:@"keydown" fromNSEvent:event];
    if (jsPrevented) return;

    // Cmd+C — copy selected text
    if ((event.modifierFlags & NSEventModifierFlagCommand) && [event.characters isEqualToString:@"c"]) {
        NSString* text = [self selectedText];
        if (text.length > 0) {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            [pb clearContents];
            [pb setString:text forType:NSPasteboardTypeString];
        }
        return;
    }

    // Keyboard scrolling
    CGFloat maxScroll = std::max(0.0, _contentHeight * _pageScale - self.bounds.size.height);
    CGFloat pageHeight = self.bounds.size.height * 0.9; // 90% of viewport for page scroll
    CGFloat arrowStep = 40.0; // pixels per arrow key press

    // Check special keys via keyCode
    unsigned short keyCode = event.keyCode;
    BOOL handled = NO;

    switch (keyCode) {
        case 126: // Up arrow
            _scrollOffset -= arrowStep;
            handled = YES;
            break;
        case 125: // Down arrow
            _scrollOffset += arrowStep;
            handled = YES;
            break;
        case 116: // Page Up
            _scrollOffset -= pageHeight;
            handled = YES;
            break;
        case 121: // Page Down
            _scrollOffset += pageHeight;
            handled = YES;
            break;
        case 115: // Home
            _scrollOffset = 0;
            handled = YES;
            break;
        case 119: // End
            _scrollOffset = maxScroll;
            handled = YES;
            break;
        default:
            break;
    }

    // Space bar: scroll down one page (Shift+Space: scroll up)
    if (!handled && [event.characters isEqualToString:@" "]) {
        if (event.modifierFlags & NSEventModifierFlagShift) {
            _scrollOffset -= pageHeight;
        } else {
            _scrollOffset += pageHeight;
        }
        handled = YES;
    }

    // Escape: clear text selection
    if (!handled && keyCode == 53) {
        if (_hasSelection) {
            _hasSelection = NO;
            _selecting = NO;
            [self setNeedsDisplay:YES];
            return;
        }
    }

    if (handled) {
        _scrollOffset = std::max(0.0, std::min(_scrollOffset, maxScroll));
        [self setNeedsDisplay:YES];
        [self.window invalidateCursorRectsForView:self];
        return;
    }

    [super keyDown:event];
}

- (void)keyUp:(NSEvent*)event {
    // Dispatch JS keyup event to the focused element.
    [self dispatchJSKeyEvent:@"keyup" fromNSEvent:event];
    [super keyUp:event];
}

#pragma mark - Inline Text Input Overlay

- (void)showTextInputOverlayWithBounds:(clever::paint::Rect)bufferBounds
                                 value:(NSString*)value
                            isPassword:(BOOL)isPassword {
    // Dismiss any existing overlay first
    [self dismissTextInputOverlay];

    _overlayBufferBounds = bufferBounds;
    _overlayIsPassword = isPassword;

    // Convert buffer-pixel coords to view coords
    CGFloat vx = (bufferBounds.x / _backingScale) * _pageScale;
    CGFloat vy = (bufferBounds.y / _backingScale) * _pageScale - _scrollOffset;
    CGFloat vw = (bufferBounds.width / _backingScale) * _pageScale;
    CGFloat vh = (bufferBounds.height / _backingScale) * _pageScale;

    // Clamp to view bounds
    if (vw < 40) vw = 40;
    if (vh < 16) vh = 16;

    NSRect fieldRect = NSMakeRect(vx, vy, vw, vh);

    if (isPassword) {
        _overlaySecureField = [[NSSecureTextField alloc] initWithFrame:fieldRect];
        _overlaySecureField.stringValue = (value ? value : @"");
        _overlaySecureField.font = [NSFont systemFontOfSize:std::max(11.0, vh * 0.55)];
        _overlaySecureField.bordered = YES;
        _overlaySecureField.bezeled = YES;
        _overlaySecureField.bezelStyle = NSTextFieldSquareBezel;
        _overlaySecureField.drawsBackground = YES;
        _overlaySecureField.backgroundColor = [NSColor textBackgroundColor];
        _overlaySecureField.textColor = [NSColor textColor];
        _overlaySecureField.focusRingType = NSFocusRingTypeExterior;
        _overlaySecureField.delegate = (id<NSTextFieldDelegate>)self;
        [self addSubview:_overlaySecureField];
        [self.window makeFirstResponder:_overlaySecureField];
    } else {
        _overlayTextField = [[NSTextField alloc] initWithFrame:fieldRect];
        _overlayTextField.stringValue = (value ? value : @"");
        _overlayTextField.font = [NSFont systemFontOfSize:std::max(11.0, vh * 0.55)];
        _overlayTextField.bordered = YES;
        _overlayTextField.bezeled = YES;
        _overlayTextField.bezelStyle = NSTextFieldSquareBezel;
        _overlayTextField.drawsBackground = YES;
        _overlayTextField.backgroundColor = [NSColor textBackgroundColor];
        _overlayTextField.textColor = [NSColor textColor];
        _overlayTextField.focusRingType = NSFocusRingTypeExterior;
        _overlayTextField.delegate = (id<NSTextFieldDelegate>)self;
        [self addSubview:_overlayTextField];
        [self.window makeFirstResponder:_overlayTextField];
    }

    NSTextField* activeField = _overlayIsPassword ? _overlaySecureField : _overlayTextField;
    dispatch_async(dispatch_get_main_queue(), ^{
        [activeField selectText:nil];
    });
}

- (void)dismissTextInputOverlay {
    if (_overlayTextField) {
        [_overlayTextField removeFromSuperview];
        _overlayTextField = nil;
    }
    if (_overlaySecureField) {
        [_overlaySecureField removeFromSuperview];
        _overlaySecureField = nil;
    }
}

- (BOOL)hasTextInputOverlay {
    return _overlayTextField != nil || _overlaySecureField != nil;
}

// NSTextFieldDelegate — overlay editing events
- (void)controlTextDidChange:(NSNotification*)notification {
    NSTextField* field = [notification object];
    if (field == _overlayTextField || field == _overlaySecureField) {
        NSString* val = [field stringValue];
        if ([_delegate respondsToSelector:@selector(renderView:didChangeInputValue:)]) {
            [_delegate renderView:self didChangeInputValue:val];
        }
    }
}

- (void)controlTextDidEndEditing:(NSNotification*)notification {
    // Called when the overlay text field loses focus (e.g. clicking elsewhere)
    NSTextField* field = [notification object];
    if (field == _overlayTextField || field == _overlaySecureField) {
        NSString* val = [field stringValue];
        if ([_delegate respondsToSelector:@selector(renderView:didFinishEditingInputWithValue:)]) {
            [_delegate renderView:self didFinishEditingInputWithValue:val];
        }
        [self dismissTextInputOverlay];
    }
}

- (NSString*)selectedText {
    if (!_hasSelection) return @"";

    float sx = std::min((float)_selectionStart.x, (float)_selectionEnd.x);
    float sy = std::min((float)_selectionStart.y, (float)_selectionEnd.y);
    float ex = std::max((float)_selectionStart.x, (float)_selectionEnd.x);
    float ey = std::max((float)_selectionStart.y, (float)_selectionEnd.y);

    // Collect text regions that overlap selection, sorted by position
    struct HitRegion {
        float y, x;
        std::string text;
    };
    std::vector<HitRegion> hits;

    for (auto& region : _textRegions) {
        auto& b = region.bounds;
        float bx = b.x / _backingScale;
        float by = b.y / _backingScale;
        float bw = b.width / _backingScale;
        float bh = b.height / _backingScale;
        if (bx + bw >= sx && bx <= ex &&
            by + bh >= sy && by <= ey) {
            hits.push_back({by, bx, region.text});
        }
    }

    // Sort by y then x (reading order)
    std::sort(hits.begin(), hits.end(), [](const HitRegion& a, const HitRegion& b) {
        if (std::abs(a.y - b.y) > 5) return a.y < b.y;
        return a.x < b.x;
    });

    NSMutableString* result = [NSMutableString string];
    float last_y = -1000;
    for (auto& h : hits) {
        if (last_y >= 0 && std::abs(h.y - last_y) > 5) {
            [result appendString:@"\n"];
        } else if (last_y >= 0) {
            [result appendString:@" "];
        }
        [result appendFormat:@"%s", h.text.c_str()];
        last_y = h.y;
    }

    return result;
}

- (void)resetCursorRects {
    [super resetCursorRects];
    // Add hand cursor for link regions (convert pixel to logical coords)
    for (auto& link : _links) {
        NSRect rect = NSMakeRect(
            (link.bounds.x / _backingScale) * _pageScale,
            (link.bounds.y / _backingScale) * _pageScale - _scrollOffset,
            (link.bounds.width / _backingScale) * _pageScale,
            (link.bounds.height / _backingScale) * _pageScale);
        [self addCursorRect:rect cursor:[NSCursor pointingHandCursor]];
    }
    // Add CSS cursor regions
    for (auto& region : _cursorRegions) {
        NSCursor* cursor = nil;
        switch (region.cursor_type) {
            case 1: cursor = [NSCursor arrowCursor]; break;              // default
            case 2: cursor = [NSCursor pointingHandCursor]; break;       // pointer
            case 3: cursor = [NSCursor IBeamCursor]; break;              // text
            case 4: cursor = [NSCursor openHandCursor]; break;           // move
            case 5: cursor = [NSCursor operationNotAllowedCursor]; break;// not-allowed
            default: continue;
        }
        NSRect rect = NSMakeRect(
            (region.bounds.x / _backingScale) * _pageScale,
            (region.bounds.y / _backingScale) * _pageScale - _scrollOffset,
            (region.bounds.width / _backingScale) * _pageScale,
            (region.bounds.height / _backingScale) * _pageScale);
        [self addCursorRect:rect cursor:cursor];
    }
}

// ============================================================================
// CSS Transition Animation: pixel-crossfade compositor
// ============================================================================

- (void)addPixelTransitions:(std::vector<PixelTransition>)transitions {
    if (transitions.empty()) return;

    CFTimeInterval now = CACurrentMediaTime();
    for (auto& t : transitions) {
        t.start_time = now;
        _activeTransitions.push_back(std::move(t));
    }

    // Start animation timer if not already running (16ms ≈ 60fps)
    if (!_transitionTimer) {
        _transitionTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 60.0
                                                            target:self
                                                          selector:@selector(transitionTimerFired:)
                                                          userInfo:nil
                                                           repeats:YES];
    }
}

- (BOOL)hasActiveTransitions {
    return !_activeTransitions.empty();
}

- (void)transitionTimerFired:(NSTimer*)timer {
    (void)timer;
    if (_activeTransitions.empty() || _basePixels.empty()) {
        [_transitionTimer invalidate];
        _transitionTimer = nil;
        return;
    }

    CFTimeInterval now = CACurrentMediaTime();

    // Create a mutable copy of the current (post-hover) pixels for compositing
    std::vector<uint8_t> composited = _basePixels;
    int bw = _baseWidth;
    int bh = _baseHeight;
    bool any_active = false;

    for (auto& tr : _activeTransitions) {
        float elapsed = static_cast<float>(now - tr.start_time) - tr.delay_s;
        if (elapsed < 0) { any_active = true; continue; } // still in delay
        float raw_t = (tr.duration_s > 0) ? (elapsed / tr.duration_s) : 1.0f;
        if (raw_t >= 1.0f) raw_t = 1.0f;
        else any_active = true;

        // Apply easing
        float t = _apply_easing(tr.timing_function, raw_t,
                                tr.bezier_x1, tr.bezier_y1,
                                tr.bezier_x2, tr.bezier_y2,
                                tr.steps_count);

        // Crossfade: blend from_pixels (old state) with base_pixels (new state) at progress t
        // result = from * (1-t) + new * t
        int x0 = static_cast<int>(tr.bounds.x);
        int y0 = static_cast<int>(tr.bounds.y);
        int tw = tr.width;
        int th = tr.height;

        for (int row = 0; row < th; ++row) {
            int buf_y = y0 + row;
            if (buf_y < 0 || buf_y >= bh) continue;
            for (int col = 0; col < tw; ++col) {
                int buf_x = x0 + col;
                if (buf_x < 0 || buf_x >= bw) continue;

                size_t buf_idx = (static_cast<size_t>(buf_y) * bw + buf_x) * 4;
                size_t from_idx = (static_cast<size_t>(row) * tw + col) * 4;
                if (from_idx + 3 >= tr.from_pixels.size()) continue;
                if (buf_idx + 3 >= composited.size()) continue;

                float inv_t = 1.0f - t;
                composited[buf_idx + 0] = static_cast<uint8_t>(
                    tr.from_pixels[from_idx + 0] * inv_t + composited[buf_idx + 0] * t);
                composited[buf_idx + 1] = static_cast<uint8_t>(
                    tr.from_pixels[from_idx + 1] * inv_t + composited[buf_idx + 1] * t);
                composited[buf_idx + 2] = static_cast<uint8_t>(
                    tr.from_pixels[from_idx + 2] * inv_t + composited[buf_idx + 2] * t);
                composited[buf_idx + 3] = static_cast<uint8_t>(
                    tr.from_pixels[from_idx + 3] * inv_t + composited[buf_idx + 3] * t);
            }
        }
    }

    // Remove completed transitions
    _activeTransitions.erase(
        std::remove_if(_activeTransitions.begin(), _activeTransitions.end(),
            [now](const PixelTransition& tr) {
                float elapsed = static_cast<float>(now - tr.start_time) - tr.delay_s;
                return elapsed >= tr.duration_s;
            }),
        _activeTransitions.end());

    // Create CGImage from composited pixels and update display
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        composited.data(), bw, bh, 8, bw * 4, cs,
        static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | kCGBitmapByteOrder32Big);
    if (ctx) {
        if (_cgImage) CGImageRelease(_cgImage);
        _cgImage = CGBitmapContextCreateImage(ctx);
        CGContextRelease(ctx);
    }
    CGColorSpaceRelease(cs);
    [self setNeedsDisplay:YES];

    if (!any_active) {
        // All transitions complete — restore the base pixels as the final state
        if (_cgImage) CGImageRelease(_cgImage);
        CGColorSpaceRef cs2 = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx2 = CGBitmapContextCreate(
            _basePixels.data(), _baseWidth, _baseHeight, 8, _baseWidth * 4, cs2,
            static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | kCGBitmapByteOrder32Big);
        if (ctx2) {
            _cgImage = CGBitmapContextCreateImage(ctx2);
            CGContextRelease(ctx2);
        }
        CGColorSpaceRelease(cs2);
        [_transitionTimer invalidate];
        _transitionTimer = nil;
        [self setNeedsDisplay:YES];
    }
}

- (const std::vector<uint8_t>&)basePixels { return _basePixels; }
- (int)baseWidth { return _baseWidth; }
- (int)baseHeight { return _baseHeight; }

@end
