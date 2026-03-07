#pragma once
#import <Cocoa/Cocoa.h>
#import "render_view.h"
#include <cmath>

struct BrowserWindowLogicalPoint {
    CGFloat x = 0.0;
    CGFloat y = 0.0;
};

static inline CGFloat browser_window_snap_document_axis_for_renderer(CGFloat documentCoord,
                                                                     CGFloat rendererScale) {
    const CGFloat normalizedScale = render_view_normalized_renderer_scale(rendererScale);
    const CGFloat normalizedCoord = std::isfinite(documentCoord)
        ? std::max<CGFloat>(0.0, documentCoord)
        : 0.0;
    return std::lround(normalizedCoord * normalizedScale) / normalizedScale;
}

static inline BrowserWindowLogicalPoint browser_window_normalize_document_scroll_point(
    CGFloat documentX,
    CGFloat documentY,
    CGFloat rendererScale) {
    return {
        browser_window_snap_document_axis_for_renderer(documentX, rendererScale),
        browser_window_snap_document_axis_for_renderer(documentY, rendererScale)
    };
}

static inline BrowserWindowLogicalPoint browser_window_normalize_document_hit_test_point(
    CGFloat documentX,
    CGFloat documentY,
    CGFloat renderedDocumentOriginX,
    CGFloat rendererScale) {
    const CGFloat normalizedScale = render_view_normalized_renderer_scale(rendererScale);
    const CGFloat normalizedOriginX = std::isfinite(renderedDocumentOriginX)
        ? std::max<CGFloat>(0.0, renderedDocumentOriginX)
        : 0.0;
    const CGFloat normalizedDocumentX = std::isfinite(documentX)
        ? std::max<CGFloat>(0.0, documentX)
        : 0.0;
    return {
        normalizedOriginX +
            std::lround((normalizedDocumentX - normalizedOriginX) * normalizedScale) / normalizedScale,
        browser_window_snap_document_axis_for_renderer(documentY, rendererScale)
    };
}

// BrowserTab encapsulates per-tab state
@interface BrowserTab : NSObject
@property (nonatomic, strong) RenderView* renderView;
@property (nonatomic, strong) NSMutableArray<NSString*>* history;
@property (nonatomic) NSInteger historyIndex;
@property (nonatomic, copy) NSString* title;
@property (nonatomic, copy) NSString* currentURL;
@property (nonatomic, strong) NSImage* faviconImage;
@property (nonatomic, copy) NSString* pageDescription;  // <meta name="description"> content
@property (nonatomic, copy) NSString* canonicalURL;     // <link rel="canonical"> href
- (instancetype)initWithFrame:(NSRect)frame;
@end

// BrowserWindowController manages the browser window with tabbed browsing
@interface BrowserWindowController : NSWindowController <NSTextFieldDelegate, RenderViewDelegate>

@property (nonatomic, strong) NSTextField* addressBar;
@property (nonatomic, strong) NSButton* backButton;
@property (nonatomic, strong) NSButton* forwardButton;
@property (nonatomic, strong) NSButton* reloadButton;
@property (nonatomic, strong) NSButton* homeButton;
@property (nonatomic, strong) NSProgressIndicator* spinner;
@property (nonatomic, strong) NSScrollView* tabBarScrollView;
@property (nonatomic, strong) NSStackView* tabBar;
@property (nonatomic, strong) NSTextField* statusBar;
@property (nonatomic, strong) NSView* progressBar;

- (void)navigateToURL:(NSString*)urlString;
- (void)focusAddressBarAndSelectAll;
- (void)renderHTML:(NSString*)html;
- (void)reload:(id)sender;
- (void)newTab;
- (void)closeCurrentTab;
- (void)switchToTabByNumber:(NSInteger)number;
- (void)nextTab;
- (void)previousTab;
- (void)viewSource;
- (void)openAddressBarModal;
- (void)goBack:(id)sender;
- (void)goForward:(id)sender;
- (void)goHome:(id)sender;
- (void)showFindBar;
- (void)zoomIn;
- (void)zoomOut;
- (void)zoomActualSize;
- (void)saveScreenshot;
- (void)printPage;
- (void)addBookmark;
- (void)removeBookmarkAtIndex:(NSInteger)index;
- (void)navigateToBookmark:(id)sender;
- (void)rebuildBookmarksMenu;

@end
