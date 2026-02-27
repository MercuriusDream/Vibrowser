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
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
  <title>Welcome to Vibrowser</title>
  <style>
    :root {
      --ink: #2f3742;
      --ink-muted: #67707d;
      --surface: #ffffff;
      --surface-2: #f5f7fa;
      --border: #d8e0ea;
      --accent: #1d5fd0;
    }
    * { box-sizing: border-box; }
    html, body { margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      color: var(--ink);
      background: var(--surface);
      line-height: 1.45;
      -webkit-font-smoothing: antialiased;
      text-rendering: optimizeLegibility;
    }
    .hero {
      background: linear-gradient(135deg, #5f74dd, #6f46ab);
      color: #fff;
      padding: 36px 24px;
    }
    .hero-inner, .content-inner {
      width: min(1080px, 100%);
      margin: 0 auto;
    }
    .hero h1 {
      margin: 0 0 10px;
      font-size: 30px;
      line-height: 1.2;
    }
    .hero p { margin: 0; color: rgba(255, 255, 255, 0.96); }
    .content { padding: 20px 24px 28px; }
    h2 {
      margin: 20px 0 12px;
      color: #213043;
      font-size: 22px;
      line-height: 1.25;
    }
    .features {
      display: flex;
      gap: 14px;
      margin: 16px 0 20px;
      flex-wrap: wrap;
    }
    .card {
      flex: 1 1 220px;
      min-width: 0;
      background: var(--surface-2);
      border: 1px solid var(--border);
      padding: 14px;
      border-radius: 10px;
      box-shadow: 0 1px 4px rgba(30, 42, 60, 0.12);
    }
    .card h3 { margin: 0 0 6px; color: #1f6199; font-size: 18px; }
    .card p { margin: 0; overflow-wrap: anywhere; }
    .stats {
      display: flex;
      gap: 12px;
      margin: 16px 0;
      flex-wrap: wrap;
    }
    .stat {
      text-align: center;
      padding: 10px 14px;
      background: linear-gradient(180deg, #f8f9fa, #e9edf1);
      border: 1px solid var(--border);
      border-radius: 10px;
      min-width: 110px;
      flex: 1 1 110px;
    }
    .stat-num { font-size: 24px; font-weight: 700; color: #2a3340; line-height: 1.2; }
    .stat-label { font-size: 12px; color: var(--ink-muted); }
    .badge {
      display: inline-block;
      background: linear-gradient(90deg, #248950, #2db867);
      color: #fff;
      padding: 3px 9px;
      border-radius: 12px;
      font-size: 12px;
      line-height: 1.2;
      vertical-align: middle;
    }
    .links {
      margin: 14px 0;
      padding: 14px;
      background: #edf3fa;
      border: 1px solid #d3deeb;
      border-radius: 10px;
    }
    .links p { margin: 0 0 8px; }
    .links a {
      display: inline-block;
      margin: 0 18px 8px 0;
      color: var(--accent);
      text-decoration: underline;
      overflow-wrap: anywhere;
    }
    .shortcuts { margin: 15px 0; overflow-x: auto; }
    .shortcuts table { width: 100%; border-collapse: collapse; min-width: 420px; }
    .shortcuts td, .shortcuts th { padding: 6px 8px; text-align: left; border-bottom: 1px solid #e3e8ef; }
    .shortcuts td:first-child, .shortcuts th:first-child { white-space: nowrap; }
    .shortcuts th { color: #2a3a4d; }
    kbd {
      background: #eef2f6;
      border: 1px solid #cfd7e2;
      border-bottom-width: 2px;
      padding: 2px 6px;
      border-radius: 4px;
      font-size: 12px;
      font-family: ui-monospace, Menlo, Consolas, monospace;
    }
    .footer {
      color: var(--ink-muted);
      margin-top: 22px;
      padding: 14px 0 4px;
      text-align: center;
      border-top: 1px solid #e6ebf1;
    }
    .footer p { margin: 0; }
    @media (max-width: 640px) {
      .hero { padding: 28px 16px; }
      .content { padding: 16px; }
      .hero h1 { font-size: 24px; }
      h2 { font-size: 20px; }
      .links a { margin-right: 12px; }
    }
  </style>
</head>
<body>
  <div class="hero">
    <div class="hero-inner">
      <h1>Vibrowser v0.7.0</h1>
      <p>A complete browser engine built from scratch in C++20. <span class="badge">3300+ tests</span></p>
    </div>
  </div>
  <div class="content">
    <div class="content-inner">
      <div class="stats">
        <div class="stat"><div class="stat-num">3300+</div><div class="stat-label">Tests</div></div>
        <div class="stat"><div class="stat-num">12</div><div class="stat-label">Libraries</div></div>
        <div class="stat"><div class="stat-num">2400+</div><div class="stat-label">Features</div></div>
        <div class="stat"><div class="stat-num">0</div><div class="stat-label">Warnings</div></div>
      </div>
      <div class="stats">
        <div class="stat"><div class="stat-num">95%</div><div class="stat-label">HTML</div></div>
        <div class="stat"><div class="stat-num">100%</div><div class="stat-label">CSS</div></div>
        <div class="stat"><div class="stat-num">89%</div><div class="stat-label">Web APIs</div></div>
      </div>
      <h2>Engine Capabilities</h2>
      <div class="features">
        <div class="card"><h3>HTML5</h3><p>Tokenizer, tree builder, &amp;entities;, forms, tables, &lt;progress&gt;, &lt;meter&gt;, &lt;details&gt;</p></div>
        <div class="card"><h3>CSS3</h3><p>Cascade, selectors, calc(), var(), gradients, transforms, float, grid, media queries</p></div>
        <div class="card"><h3>Layout</h3><p>Block, inline, flex-wrap, float, grid, columns, tables, margin collapsing</p></div>
        <div class="card"><h3>SVG</h3><p>Shapes, paths, text, tspan, gradients, viewBox, transforms, fill/stroke, dasharray</p></div>
        <div class="card"><h3>Rendering</h3><p>CoreText <strong>bold</strong>/<em>italic</em>, filters, blend modes, clip-path, WebP/HEIC images</p></div>
        <div class="card"><h3>Interactivity</h3><p>:hover/:focus transitions, text input, form controls, #anchor scroll</p></div>
      </div>
      <h2>Try It Out</h2>
      <div class="links">
        <p><strong>Click a link to navigate:</strong></p>
        <a href="http://example.com">Example.com</a>
        <a href="https://info.cern.ch">First Website Ever</a>
        <a href="http://motherfuckingwebsite.com">MF Website</a>
      </div>
      <div class="shortcuts">
        <h2>Keyboard Shortcuts</h2>
        <table>
          <tr><th>Shortcut</th><th>Action</th></tr>
          <tr><td><kbd>Cmd+L</kbd></td><td>Focus address bar</td></tr>
          <tr><td><kbd>Cmd+T</kbd></td><td>New tab</td></tr>
          <tr><td><kbd>Cmd+W</kbd></td><td>Close tab</td></tr>
          <tr><td><kbd>Cmd+1</kbd>-<kbd>8</kbd></td><td>Switch to tab N</td></tr>
          <tr><td><kbd>Cmd+9</kbd></td><td>Switch to last tab</td></tr>
          <tr><td><kbd>Cmd+Shift+]</kbd></td><td>Next tab</td></tr>
          <tr><td><kbd>Cmd+Shift+[</kbd></td><td>Previous tab</td></tr>
          <tr><td><kbd>Cmd+R</kbd></td><td>Reload page</td></tr>
          <tr><td><kbd>Cmd+[</kbd></td><td>Back</td></tr>
          <tr><td><kbd>Cmd+]</kbd></td><td>Forward</td></tr>
          <tr><td><kbd>Cmd+F</kbd></td><td>Find in page</td></tr>
          <tr><td><kbd>Cmd+U</kbd></td><td>View source</td></tr>
          <tr><td><kbd>Cmd+C</kbd></td><td>Copy selected text</td></tr>
          <tr><td><kbd>Cmd++</kbd></td><td>Zoom in</td></tr>
          <tr><td><kbd>Cmd+-</kbd></td><td>Zoom out</td></tr>
          <tr><td><kbd>Cmd+0</kbd></td><td>Actual size</td></tr>
          <tr><td><kbd>Cmd+S</kbd></td><td>Save screenshot</td></tr>
          <tr><td><kbd>Cmd+P</kbd></td><td>Print / Save as PDF</td></tr>
          <tr><td><kbd>Cmd+D</kbd></td><td>Add bookmark</td></tr>
        </table>
      </div>
      <div class="footer"><p>Vibrowser &mdash; built with the Vibrowser Engine</p></div>
    </div>
  </div>
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
