#import <Cocoa/Cocoa.h>
#import "browser_window.h"
#include <string>

// -----------------------------------------------------------
// CleverAppDelegate — manages the application lifecycle
// -----------------------------------------------------------
@interface CleverAppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) BrowserWindowController* browserController;
@end

// Global welcome HTML so both main and browser_window can access it
std::string getWelcomeHTML() {
    return std::string([@"<html><head><title>Welcome to Clever</title><style>"
        @"body { font-family: sans-serif; margin: 0; color: #333; }"
        @".hero { background: linear-gradient(135deg, #667eea, #764ba2); color: white; padding: 40px 30px; }"
        @".hero h1 { color: white; font-size: 28px; }"
        @".hero p { color: white; opacity: 0.9; }"
        @".content { padding: 20px 30px; }"
        @"h2 { color: #2c3e50; }"
        @".features { display: flex; gap: 15px; margin: 20px 0; flex-wrap: wrap; }"
        @".card { background-color: #f8f9fa; padding: 15px; flex-grow: 1; border-radius: 8px; "
        @"  box-shadow: 2px 2px 8px #ccc; min-width: 180px; }"
        @".card h3 { color: #2980b9; }"
        @".stats { display: flex; gap: 20px; margin: 20px 0; flex-wrap: wrap; }"
        @".stat { text-align: center; padding: 12px 20px; background: linear-gradient(180deg, #f8f9fa, #e9ecef); "
        @"  border-radius: 8px; min-width: 100px; }"
        @".stat-num { font-size: 24px; font-weight: bold; color: #2c3e50; }"
        @".stat-label { font-size: 11px; color: #7f8c8d; }"
        @".badge { background: linear-gradient(90deg, #27ae60, #2ecc71); color: white; padding: 3px 10px; border-radius: 12px; }"
        @".new-tag { background: linear-gradient(90deg, #e74c3c, #e67e22); color: white; padding: 2px 8px; border-radius: 8px; }"
        @".links { margin: 15px 0; padding: 15px; background-color: #f0f4f8; border-radius: 8px; }"
        @".links a { margin-right: 20px; }"
        @".shortcuts { margin: 15px 0; }"
        @".shortcuts table { width: 100%; }"
        @".shortcuts td { padding: 4px 8px; }"
        @".shortcuts th { padding: 4px 8px; text-align: left; color: #2c3e50; }"
        @"kbd { background-color: #e9ecef; padding: 2px 6px; border-radius: 4px; font-size: 12px; }"
        @".footer { color: #7f8c8d; margin-top: 20px; padding: 15px 0; text-align: center; }"
        @"</style></head><body>"
        @"<div class=\"hero\">"
        @"<h1>Clever Browser v0.7.0</h1>"
        @"<p>A complete browser engine built from scratch in C++20. "
        @"<span class=\"badge\">3300+ tests</span></p>"
        @"</div>"
        @"<div class=\"content\">"
        @"<div class=\"stats\">"
        @"<div class=\"stat\"><div class=\"stat-num\">3300+</div><div class=\"stat-label\">Tests</div></div>"
        @"<div class=\"stat\"><div class=\"stat-num\">12</div><div class=\"stat-label\">Libraries</div></div>"
        @"<div class=\"stat\"><div class=\"stat-num\">2400+</div><div class=\"stat-label\">Features</div></div>"
        @"<div class=\"stat\"><div class=\"stat-num\">0</div><div class=\"stat-label\">Warnings</div></div>"
        @"</div>"
        @"<div class=\"stats\">"
        @"<div class=\"stat\"><div class=\"stat-num\">95%</div><div class=\"stat-label\">HTML</div></div>"
        @"<div class=\"stat\"><div class=\"stat-num\">100%</div><div class=\"stat-label\">CSS</div></div>"
        @"<div class=\"stat\"><div class=\"stat-num\">89%</div><div class=\"stat-label\">Web APIs</div></div>"
        @"</div>"
        @"<h2>Engine Capabilities</h2>"
        @"<div class=\"features\">"
        @"<div class=\"card\"><h3>HTML5</h3><p>Tokenizer, tree builder, &amp;entities;, forms, "
        @"tables, &lt;progress&gt;, &lt;meter&gt;, &lt;details&gt;</p></div>"
        @"<div class=\"card\"><h3>CSS3</h3><p>Cascade, selectors, calc(), var(), "
        @"gradients, transforms, float, grid, media queries</p></div>"
        @"<div class=\"card\"><h3>Layout</h3><p>Block, inline, flex-wrap, float, "
        @"grid, columns, tables, margin collapsing</p></div>"
        @"<div class=\"card\"><h3>SVG</h3><p>Shapes, paths, text, tspan, gradients, "
        @"viewBox, transforms, fill/stroke, dasharray</p></div>"
        @"<div class=\"card\"><h3>Rendering</h3><p>CoreText <strong>bold</strong>/<em>italic</em>, "
        @"filters, blend modes, clip-path, WebP/HEIC images</p></div>"
        @"<div class=\"card\"><h3>Interactivity</h3><p>:hover/:focus transitions, "
        @"text input, form controls, #anchor scroll</p></div>"
        @"</div>"
        @"<h2>Try It Out</h2>"
        @"<div class=\"links\">"
        @"<p><strong>Click a link to navigate:</strong></p>"
        @"<a href=\"http://example.com\">Example.com</a>"
        @"<a href=\"https://info.cern.ch\">First Website Ever</a>"
        @"<a href=\"http://motherfuckingwebsite.com\">MF Website</a>"
        @"</div>"
        @"<div class=\"shortcuts\">"
        @"<h2>Keyboard Shortcuts</h2>"
        @"<table>"
        @"<tr><th>Shortcut</th><th>Action</th></tr>"
        @"<tr><td><kbd>Cmd+L</kbd></td><td>Focus address bar</td></tr>"
        @"<tr><td><kbd>Cmd+T</kbd></td><td>New tab</td></tr>"
        @"<tr><td><kbd>Cmd+W</kbd></td><td>Close tab</td></tr>"
        @"<tr><td><kbd>Cmd+1</kbd>-<kbd>8</kbd></td><td>Switch to tab N</td></tr>"
        @"<tr><td><kbd>Cmd+9</kbd></td><td>Switch to last tab</td></tr>"
        @"<tr><td><kbd>Cmd+Shift+]</kbd></td><td>Next tab</td></tr>"
        @"<tr><td><kbd>Cmd+Shift+[</kbd></td><td>Previous tab</td></tr>"
        @"<tr><td><kbd>Cmd+R</kbd></td><td>Reload page</td></tr>"
        @"<tr><td><kbd>Cmd+[</kbd></td><td>Back</td></tr>"
        @"<tr><td><kbd>Cmd+]</kbd></td><td>Forward</td></tr>"
        @"<tr><td><kbd>Cmd+F</kbd></td><td>Find in page</td></tr>"
        @"<tr><td><kbd>Cmd+U</kbd></td><td>View source</td></tr>"
        @"<tr><td><kbd>Cmd+C</kbd></td><td>Copy selected text</td></tr>"
        @"<tr><td><kbd>Cmd++</kbd></td><td>Zoom in</td></tr>"
        @"<tr><td><kbd>Cmd+-</kbd></td><td>Zoom out</td></tr>"
        @"<tr><td><kbd>Cmd+0</kbd></td><td>Actual size</td></tr>"
        @"<tr><td><kbd>Cmd+S</kbd></td><td>Save screenshot</td></tr>"
        @"<tr><td><kbd>Cmd+P</kbd></td><td>Print / Save as PDF</td></tr>"
        @"<tr><td><kbd>Cmd+D</kbd></td><td>Add bookmark</td></tr>"
        @"</table>"
        @"</div>"
        @"<div class=\"footer\"><p>Clever Browser &mdash; built with the Clever Engine</p></div>"
        @"</div>"
        @"</body></html>" UTF8String]);
}

@implementation CleverAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    [self setupMenuBar];

    _browserController = [[BrowserWindowController alloc] init];
    [_browserController showWindow:nil];
    [_browserController.window makeKeyAndOrderFront:nil];
    [_browserController focusAddressBarAndSelectAll];

    // Render a welcome page on startup
    std::string html = getWelcomeHTML();
    [_browserController renderHTML:[NSString stringWithUTF8String:html.c_str()]];

    // Populate bookmarks menu with saved bookmarks
    [_browserController rebuildBookmarksMenu];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}

- (void)setupMenuBar {
    NSMenu* mainMenu = [[NSMenu alloc] init];

    // App menu
    NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
    NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"Clever"];
    [appMenu addItemWithTitle:@"About Clever Browser"
                       action:@selector(orderFrontStandardAboutPanel:)
                keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Quit Clever Browser"
                       action:@selector(terminate:)
                keyEquivalent:@"q"];
    appMenuItem.submenu = appMenu;
    [mainMenu addItem:appMenuItem];

    // File menu
    NSMenuItem* fileMenuItem = [[NSMenuItem alloc] init];
    NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    [fileMenu addItemWithTitle:@"New Tab"
                        action:@selector(newTabAction:)
                 keyEquivalent:@"t"];
    [fileMenu addItemWithTitle:@"Close Tab"
                        action:@selector(closeTabAction:)
                 keyEquivalent:@"w"];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"New Window"
                        action:@selector(newWindow:)
                 keyEquivalent:@"n"];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"Save Screenshot"
                        action:@selector(saveScreenshotAction:)
                 keyEquivalent:@"s"];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"Print..."
                        action:@selector(printPageAction:)
                 keyEquivalent:@"p"];
    fileMenuItem.submenu = fileMenu;
    [mainMenu addItem:fileMenuItem];

    // Tab menu (between File and Edit)
    NSMenuItem* tabMenuItem = [[NSMenuItem alloc] init];
    NSMenu* tabMenu = [[NSMenu alloc] initWithTitle:@"Tab"];

    // Next Tab: Cmd+Shift+]
    NSMenuItem* nextTabItem = [[NSMenuItem alloc] initWithTitle:@"Show Next Tab"
                                                        action:@selector(nextTabAction:)
                                                 keyEquivalent:@"]"];
    nextTabItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    [tabMenu addItem:nextTabItem];

    // Previous Tab: Cmd+Shift+[
    NSMenuItem* prevTabItem = [[NSMenuItem alloc] initWithTitle:@"Show Previous Tab"
                                                        action:@selector(previousTabAction:)
                                                 keyEquivalent:@"["];
    prevTabItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    [tabMenu addItem:prevTabItem];

    // Next Tab alternate: Cmd+Option+Right
    NSMenuItem* nextTabAlt = [[NSMenuItem alloc] initWithTitle:@"Show Next Tab"
                                                       action:@selector(nextTabAction:)
                                                keyEquivalent:[NSString stringWithFormat:@"%C", (unichar)NSRightArrowFunctionKey]];
    nextTabAlt.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
    nextTabAlt.alternate = YES;
    [tabMenu addItem:nextTabAlt];

    // Previous Tab alternate: Cmd+Option+Left
    NSMenuItem* prevTabAlt = [[NSMenuItem alloc] initWithTitle:@"Show Previous Tab"
                                                       action:@selector(previousTabAction:)
                                                keyEquivalent:[NSString stringWithFormat:@"%C", (unichar)NSLeftArrowFunctionKey]];
    prevTabAlt.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
    prevTabAlt.alternate = YES;
    [tabMenu addItem:prevTabAlt];

    [tabMenu addItem:[NSMenuItem separatorItem]];

    // Cmd+1 through Cmd+9: Switch to tab N
    for (int i = 1; i <= 9; i++) {
        NSString* title;
        if (i == 9) {
            title = @"Switch to Last Tab";
        } else {
            title = [NSString stringWithFormat:@"Switch to Tab %d", i];
        }
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                      action:@selector(switchToTabAction:)
                                               keyEquivalent:[NSString stringWithFormat:@"%d", i]];
        item.tag = i;
        [tabMenu addItem:item];
    }

    tabMenuItem.submenu = tabMenu;
    [mainMenu addItem:tabMenuItem];

    // Edit menu
    NSMenuItem* editMenuItem = [[NSMenuItem alloc] init];
    NSMenu* editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    [editMenu addItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
    [editMenu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
    [editMenu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
    [editMenu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
    editMenuItem.submenu = editMenu;
    [mainMenu addItem:editMenuItem];

    // View menu
    NSMenuItem* viewMenuItem = [[NSMenuItem alloc] init];
    NSMenu* viewMenu = [[NSMenu alloc] initWithTitle:@"View"];

    NSMenuItem* focusAddr = [[NSMenuItem alloc] initWithTitle:@"Focus Address Bar"
                                                      action:@selector(focusAddressBar:)
                                               keyEquivalent:@"l"];
    [viewMenu addItem:focusAddr];

    NSMenuItem* openModalAddr = [[NSMenuItem alloc] initWithTitle:@"Open URL..."
                                                        action:@selector(openAddressModal:)
                                                 keyEquivalent:@"L"];
    openModalAddr.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    [viewMenu addItem:openModalAddr];

    NSMenuItem* reloadItem = [[NSMenuItem alloc] initWithTitle:@"Reload"
                                                       action:@selector(reloadPage:)
                                                keyEquivalent:@"r"];
    [viewMenu addItem:reloadItem];

    [viewMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* viewSourceItem = [[NSMenuItem alloc] initWithTitle:@"View Source"
                                                           action:@selector(viewSourceAction:)
                                                    keyEquivalent:@"u"];
    [viewMenu addItem:viewSourceItem];

    [viewMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* findItem = [[NSMenuItem alloc] initWithTitle:@"Find in Page"
                                                     action:@selector(findInPage:)
                                              keyEquivalent:@"f"];
    [viewMenu addItem:findItem];

    [viewMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* zoomInItem = [[NSMenuItem alloc] initWithTitle:@"Zoom In"
                                                       action:@selector(zoomInAction:)
                                                keyEquivalent:@"+"];
    [viewMenu addItem:zoomInItem];

    // Cmd+= also triggers zoom in (same physical key without Shift on US keyboards)
    NSMenuItem* zoomInAlt = [[NSMenuItem alloc] initWithTitle:@"Zoom In"
                                                      action:@selector(zoomInAction:)
                                               keyEquivalent:@"="];
    zoomInAlt.alternate = YES;
    zoomInAlt.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    [viewMenu addItem:zoomInAlt];

    NSMenuItem* zoomOutItem = [[NSMenuItem alloc] initWithTitle:@"Zoom Out"
                                                        action:@selector(zoomOutAction:)
                                                 keyEquivalent:@"-"];
    [viewMenu addItem:zoomOutItem];

    NSMenuItem* actualSizeItem = [[NSMenuItem alloc] initWithTitle:@"Actual Size"
                                                           action:@selector(zoomActualSizeAction:)
                                                    keyEquivalent:@"0"];
    [viewMenu addItem:actualSizeItem];

    viewMenuItem.submenu = viewMenu;
    [mainMenu addItem:viewMenuItem];

    // Bookmarks menu (between View and History)
    NSMenuItem* bookmarksMenuItem = [[NSMenuItem alloc] init];
    NSMenu* bookmarksMenu = [[NSMenu alloc] initWithTitle:@"Bookmarks"];
    [bookmarksMenu addItemWithTitle:@"Add Bookmark"
                             action:@selector(addBookmarkAction:)
                      keyEquivalent:@"d"];
    [bookmarksMenu addItem:[NSMenuItem separatorItem]];
    // Dynamic bookmark items will be added by rebuildBookmarksMenu
    bookmarksMenuItem.submenu = bookmarksMenu;
    [mainMenu addItem:bookmarksMenuItem];

    // History menu
    NSMenuItem* histMenuItem = [[NSMenuItem alloc] init];
    NSMenu* histMenu = [[NSMenu alloc] initWithTitle:@"History"];

    NSMenuItem* backItem = [[NSMenuItem alloc] initWithTitle:@"Back"
                                                     action:@selector(goBackAction:)
                                              keyEquivalent:@"["];
    [histMenu addItem:backItem];

    NSMenuItem* fwdItem = [[NSMenuItem alloc] initWithTitle:@"Forward"
                                                    action:@selector(goForwardAction:)
                                             keyEquivalent:@"]"];
    [histMenu addItem:fwdItem];

    [histMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* homeItem = [[NSMenuItem alloc] initWithTitle:@"Home"
                                                     action:@selector(goHomeAction:)
                                              keyEquivalent:@"h"];
    homeItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    [histMenu addItem:homeItem];

    histMenuItem.submenu = histMenu;
    [mainMenu addItem:histMenuItem];

    [NSApp setMainMenu:mainMenu];
}

- (void)focusAddressBar:(id)sender {
    (void)sender;
    [_browserController openAddressBarModal];
}

- (void)openAddressModal:(id)sender {
    (void)sender;
    [_browserController openAddressBarModal];
}

- (void)reloadPage:(id)sender {
    [_browserController reload:sender];
}

- (void)newTabAction:(id)sender {
    (void)sender;
    [_browserController newTab];
}

- (void)closeTabAction:(id)sender {
    (void)sender;
    [_browserController closeCurrentTab];
}

- (void)nextTabAction:(id)sender {
    (void)sender;
    [_browserController nextTab];
}

- (void)previousTabAction:(id)sender {
    (void)sender;
    [_browserController previousTab];
}

- (void)switchToTabAction:(id)sender {
    NSMenuItem* item = (NSMenuItem*)sender;
    [_browserController switchToTabByNumber:item.tag];
}

- (void)viewSourceAction:(id)sender {
    (void)sender;
    [_browserController viewSource];
}

- (void)goBackAction:(id)sender {
    (void)sender;
    [_browserController goBack:nil];
}

- (void)goForwardAction:(id)sender {
    (void)sender;
    [_browserController goForward:nil];
}

- (void)goHomeAction:(id)sender {
    (void)sender;
    [_browserController goHome:nil];
}

- (void)findInPage:(id)sender {
    (void)sender;
    [_browserController showFindBar];
}

- (void)zoomInAction:(id)sender {
    (void)sender;
    [_browserController zoomIn];
}

- (void)zoomOutAction:(id)sender {
    (void)sender;
    [_browserController zoomOut];
}

- (void)zoomActualSizeAction:(id)sender {
    (void)sender;
    [_browserController zoomActualSize];
}

- (void)newWindow:(id)sender {
    (void)sender;
    [_browserController.window makeKeyAndOrderFront:nil];
}

- (void)saveScreenshotAction:(id)sender {
    (void)sender;
    [_browserController saveScreenshot];
}

- (void)printPageAction:(id)sender {
    (void)sender;
    [_browserController printPage];
}

- (void)addBookmarkAction:(id)sender {
    (void)sender;
    [_browserController addBookmark];
}

- (void)bookmarkMenuItemClicked:(id)sender {
    (void)sender;
    [_browserController navigateToBookmark:sender];
}

- (void)removeAllBookmarksAction:(id)sender {
    (void)sender;
    // Remove all bookmarks one by one from the end
    while (YES) {
        // Keep removing index 0 until empty; removeBookmarkAtIndex handles bounds
        [_browserController removeBookmarkAtIndex:0];
        // Check if the bookmarks menu only has the static items left
        NSMenu* mainMenu = [NSApp mainMenu];
        NSMenuItem* bmItem = nil;
        for (NSMenuItem* item in mainMenu.itemArray) {
            if ([item.submenu.title isEqualToString:@"Bookmarks"]) {
                bmItem = item;
                break;
            }
        }
        if (!bmItem || bmItem.submenu.numberOfItems <= 2) break;
    }
}

@end

// -----------------------------------------------------------
// main() — standard macOS application entry point
// -----------------------------------------------------------
int main(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;

    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        CleverAppDelegate* delegate = [[CleverAppDelegate alloc] init];
        [app setDelegate:delegate];

        [app activateIgnoringOtherApps:YES];
        [app run];
    }

    return 0;
}
