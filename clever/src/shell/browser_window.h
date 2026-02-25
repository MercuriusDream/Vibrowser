#pragma once
#import <Cocoa/Cocoa.h>
#import "render_view.h"

// BrowserTab encapsulates per-tab state
@interface BrowserTab : NSObject
@property (nonatomic, strong) RenderView* renderView;
@property (nonatomic, strong) NSMutableArray<NSString*>* history;
@property (nonatomic) NSInteger historyIndex;
@property (nonatomic, copy) NSString* title;
@property (nonatomic, copy) NSString* currentURL;
@property (nonatomic, strong) NSImage* faviconImage;
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
