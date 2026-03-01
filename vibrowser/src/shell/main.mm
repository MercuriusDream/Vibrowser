#import <Cocoa/Cocoa.h>
#import "browser_window.h"
#include <string>

// -----------------------------------------------------------
// VibrowserAppDelegate — manages the application lifecycle
// -----------------------------------------------------------
@interface VibrowserAppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) BrowserWindowController* browserController;
@end

// Global welcome HTML so both main and browser_window can access it
std::string getWelcomeHTML() {
    return R"(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Welcome to Vibrowser</title>
  <style>
    * { box-sizing: border-box; }
    html, body { margin: 0; padding: 0; }
    body {
      font-family: sans-serif;
      color: #1f2937;
      line-height: 1.5;
      -webkit-font-smoothing: antialiased;
      background: #f3f6fb;
    }
    header, main, section { display: block; }
    .hero {
      background: #2f5ec8;
      color: #ffffff;
      padding: 24px 16px;
    }
    .hero h1 { margin: 0 0 6px; font-size: 30px; line-height: 1.2; }
    .hero p { margin: 0; color: #eaf1ff; }
    .content {
      width: 100%;
      background: #ffffff;
      border: 1px solid #d7dee7;
      border-left-width: 0;
      border-left-style: none;
      border-right-width: 0;
      border-right-style: none;
    }
    .section {
      padding: 14px 16px;
      border-bottom-width: 1px;
      border-bottom-style: solid;
      border-bottom-color: #ebf0f5;
    }
    .section:last-child {
      border-bottom-width: 0;
      border-bottom-style: none;
    }
    h2 { margin: 0 0 10px; font-size: 20px; color: #1c3555; line-height: 1.3; }
    p { margin: 0; color: #5f6b7a; }
    table { width: 100%; border-collapse: collapse; }
    th, td { border: 1px solid #e4ebf2; padding: 8px; text-align: left; vertical-align: top; }
    th { background: #f5f8fc; color: #304761; }
    a { color: #2563eb; text-decoration: underline; }
    kbd {
      background: #eef2f6;
      border: 1px solid #cfd7e2;
      border-bottom-width: 2px;
      padding: 2px 6px;
      border-radius: 4px;
      font-size: 12px;
      font-family: monospace;
    }
    .footer { text-align: center; color: #5f6b7a; padding: 12px 16px 14px; font-size: 13px; }
    @media (max-width: 640px) {
      .hero { padding: 26px 14px; }
      .hero h1 { font-size: 28px; }
      .section { padding: 12px; }
    }
  </style>
</head>
<body>
  <header class="hero">
    <h1>Vibrowser v0.7.0</h1>
    <p>A browser engine built from scratch in C++20. 3300+ tests.</p>
  </header>
  <main class="content">
    <section class="section">
      <h2>Status Snapshot</h2>
      <table>
        <tr><th>Metric</th><th>Value</th></tr>
        <tr><td>Tests</td><td>3300+</td></tr>
        <tr><td>Features</td><td>2400+</td></tr>
        <tr><td>HTML Coverage</td><td>95%</td></tr>
        <tr><td>CSS Coverage</td><td>100%</td></tr>
        <tr><td>Web APIs</td><td>89%</td></tr>
      </table>
    </section>
    <section class="section">
      <h2>Engine Capabilities</h2>
      <table>
        <tr><th>Area</th><th>Coverage</th></tr>
        <tr><td>HTML5</td><td>Tokenizer, parser, forms, tables, details and summary, semantic tags</td></tr>
        <tr><td>CSS3</td><td>Cascade, selectors, variables, transforms, media rules</td></tr>
        <tr><td>Layout</td><td>Block and inline flow, flex-wrap, floats, grid features, columns</td></tr>
        <tr><td>Rendering</td><td>Text shaping, image decode, filters, blending, clip paths and SVG primitives</td></tr>
        <tr><td>Interactivity</td><td>Mouse events, keyboard input, forms, fragment navigation</td></tr>
      </table>
    </section>
    <section class="section">
      <h2>Try Navigation</h2>
      <p><a href="http://example.com">Example.com</a></p>
      <p><a href="https://info.cern.ch">First Website Ever</a></p>
      <p><a href="http://motherfuckingwebsite.com">MF Website</a></p>
    </section>
    <section class="section">
      <h2>Keyboard Shortcuts</h2>
      <table>
        <tr><th>Shortcut</th><th>Action</th></tr>
        <tr><td><kbd>Cmd+L</kbd></td><td>Focus address bar</td></tr>
        <tr><td><kbd>Cmd+T</kbd></td><td>New tab</td></tr>
        <tr><td><kbd>Cmd+W</kbd></td><td>Close tab</td></tr>
        <tr><td><kbd>Cmd+R</kbd></td><td>Reload page</td></tr>
        <tr><td><kbd>Cmd+[</kbd> / <kbd>Cmd+]</kbd></td><td>Back / Forward</td></tr>
      </table>
    </section>
    <div class="footer">Vibrowser Engine on macOS</div>
  </main>
</body>
</html>
)";
}

@implementation VibrowserAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    [self setupMenuBar];

    _browserController = [[BrowserWindowController alloc] init];
    [_browserController showWindow:nil];
    [_browserController.window makeKeyAndOrderFront:nil];
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
    [_browserController focusAddressBarAndSelectAll];

    // Check for command-line URL argument
    NSArray* args = [[NSProcessInfo processInfo] arguments];
    NSString* cmdLineURL = nil;
    for (NSUInteger i = 1; i < args.count; i++) {
        NSString* arg = args[i];
        if ([arg hasPrefix:@"http://"] || [arg hasPrefix:@"https://"] || [arg hasPrefix:@"file://"]) {
            cmdLineURL = arg;
            break;
        }
    }

    if (cmdLineURL) {
        // Show welcome first, then navigate after window is fully set up
        std::string html = getWelcomeHTML();
        [_browserController renderHTML:[NSString stringWithUTF8String:html.c_str()]];
        NSString* url = [cmdLineURL copy];
        BrowserWindowController* ctrl = _browserController;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [ctrl navigateToURL:url];
        });
    } else {
        std::string html = getWelcomeHTML();
        [_browserController renderHTML:[NSString stringWithUTF8String:html.c_str()]];
    }

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
    NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"Vibrowser"];
    [appMenu addItemWithTitle:@"About Vibrowser"
                       action:@selector(orderFrontStandardAboutPanel:)
                keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Quit Vibrowser"
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
    [_browserController focusAddressBarAndSelectAll];
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

        VibrowserAppDelegate* delegate = [[VibrowserAppDelegate alloc] init];
        [app setDelegate:delegate];

        [app activateIgnoringOtherApps:YES];
        [app run];
    }

    return 0;
}
