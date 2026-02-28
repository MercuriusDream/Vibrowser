#import "browser_window.h"
#import "render_view.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <QuartzCore/QuartzCore.h>

#include <clever/paint/render_pipeline.h>
#include <clever/paint/software_renderer.h>
#include <clever/js/js_dom_bindings.h>
#include <clever/js/js_engine.h>
#include <clever/js/js_timers.h>
#include <clever/html/tree_builder.h>
#include <clever/net/http_client.h>
#include <clever/net/request.h>
#include <clever/net/cookie_jar.h>
#include <clever/url/url.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>

static const CGFloat kToolbarHeight = 44.0;
static const CGFloat kTabBarHeight = 34.0;
static const CGFloat kToolbarButtonWidth = 30.0;
static const CGFloat kToolbarButtonHeight = 28.0;
static const CGFloat kToolbarControlGap = 8.0;
static const CGFloat kChromeHorizontalInset = 12.0;
static const CGFloat kTabPlusButtonWidth = 28.0;
static const CGFloat kAddressBarMinWidth = 120.0;
static const CGFloat kAddressBarHeight = 30.0;
static const CGFloat kToolbarActionButtonWidth = 34.0;
static const CGFloat kToolbarActionButtonGap = 6.0;
static const CGFloat kTrafficLightClearance = 12.0;

static NSString* const kBrowserAppName = @"Vibrowser";
static NSString* const kBrowserFocusAttr = @"data-vibrowser-focus";
static NSString* const kBrowserHoverAttr = @"data-vibrowser-hover";

static NSImage* browser_symbol_image(NSString* system_name) {
    if (@available(macOS 11.0, *)) {
        NSImage* image = [NSImage imageWithSystemSymbolName:system_name
                                       accessibilityDescription:nil];
        if (image) return image;
    }
    return nil;
}

@interface AddressBarTextField : NSTextField
@end

@implementation AddressBarTextField
- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)canBecomeKeyView {
    return YES;
}

- (void)mouseDown:(NSEvent*)event {
    [super mouseDown:event];
    if (self.window) {
        [self.window makeFirstResponder:self];
    }
}
@end

// =============================================================================
// DOM tree serialization — serialize SimpleNode tree back to HTML string.
// Used to persist form control state changes (select, checkbox, radio)
// into currentHTML so that re-render reflects the updated state.
// =============================================================================

static std::string serialize_dom_node(const clever::html::SimpleNode* node) {
    if (!node) return "";
    if (node->type == clever::html::SimpleNode::Text) {
        return node->data;
    }
    if (node->type == clever::html::SimpleNode::Comment) {
        return "<!--" + node->data + "-->";
    }
    if (node->type == clever::html::SimpleNode::DocumentType) {
        return "<!DOCTYPE " + node->doctype_name + ">";
    }
    if (node->type != clever::html::SimpleNode::Element) {
        // Document node — just serialize children
        std::string result;
        for (auto& child : node->children) {
            result += serialize_dom_node(child.get());
        }
        return result;
    }

    // Element type
    std::string result = "<" + node->tag_name;
    for (auto& attr : node->attributes) {
        if (attr.value.empty()) {
            // Boolean attribute (e.g., checked, selected, disabled)
            result += " " + attr.name;
        } else {
            result += " " + attr.name + "=\"" + attr.value + "\"";
        }
    }
    result += ">";

    // Void elements (self-closing, no end tag)
    static const char* void_tags[] = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "param", "source", "track", "wbr", nullptr
    };
    for (int i = 0; void_tags[i]; i++) {
        if (node->tag_name == void_tags[i]) return result;
    }

    for (auto& child : node->children) {
        result += serialize_dom_node(child.get());
    }
    result += "</" + node->tag_name + ">";
    return result;
}

// Helper: set or add an attribute on a SimpleNode
static void set_attr(clever::html::SimpleNode* node, const std::string& name, const std::string& value) {
    for (auto& attr : node->attributes) {
        if (attr.name == name) {
            attr.value = value;
            return;
        }
    }
    node->attributes.push_back({name, value});
}

// Helper: remove an attribute from a SimpleNode
static void remove_attr(clever::html::SimpleNode* node, const std::string& name) {
    auto& attrs = node->attributes;
    attrs.erase(std::remove_if(attrs.begin(), attrs.end(),
        [&](const clever::html::Attribute& a) { return a.name == name; }),
        attrs.end());
}

// Helper: check if a SimpleNode has a given attribute
static bool has_attr(const clever::html::SimpleNode* node, const std::string& name) {
    for (auto& attr : node->attributes) {
        if (attr.name == name) return true;
    }
    return false;
}

// Helper: get attribute value (empty string if not found)
static std::string get_attr(const clever::html::SimpleNode* node, const std::string& name) {
    for (auto& attr : node->attributes) {
        if (attr.name == name) return attr.value;
    }
    return "";
}

static NSString* normalized_tab_title(id title) {
    if (!title || ![title isKindOfClass:[NSString class]]) return @"";
    NSString* normalized = [title copy];
    if (!normalized || normalized.length == 0) return @"";
    return normalized;
}

static constexpr const char kBrowserUserAgent[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Vibrowser/0.7.0 Safari/537.36";

static std::string ascii_lower(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

static bool page_uses_hover_state(const std::string& html) {
    if (html.empty()) return false;
    std::string lower = ascii_lower(html);
    return lower.find(":hover") != std::string::npos ||
           lower.find("onmouseover=") != std::string::npos ||
           lower.find("onmouseenter=") != std::string::npos;
}

static bool looks_like_duckduckgo_host(const std::string& host_lower) {
    return host_lower == "duckduckgo.com" || host_lower == "www.duckduckgo.com";
}

static bool looks_like_google_host(const std::string& host_lower) {
    return host_lower == "google.com" || host_lower == "www.google.com" ||
           host_lower.rfind("google.", 0) == 0 ||
           host_lower.rfind("www.google.", 0) == 0;
}

static bool is_google_search_compat_path(const std::string& path) {
    if (path.empty() || path == "/") return true;
    const std::string path_lower = ascii_lower(path);
    return path_lower == "/search" || path_lower == "/webhp";
}

static bool query_has_key(const std::string& query, const std::string& key) {
    std::size_t start = 0;
    while (start <= query.size()) {
        const std::size_t end = query.find('&', start);
        const std::string token =
            query.substr(start, (end == std::string::npos ? query.size() : end) - start);
        if (!token.empty()) {
            const std::size_t equals = token.find('=');
            const std::string token_key = token.substr(0, equals);
            if (token_key == key) {
                return true;
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return false;
}

static void ensure_query_param(std::string& query, const std::string& key, const std::string& value) {
    if (query_has_key(query, key)) {
        return;
    }
    if (!query.empty()) {
        query.push_back('&');
    }
    query += key;
    query.push_back('=');
    query += value;
}

static std::string normalize_url_for_compat(const std::string& input_url) {
    auto parsed_opt = clever::url::parse(input_url);
    if (!parsed_opt) {
        return input_url;
    }

    clever::url::URL parsed = *parsed_opt;
    const std::string scheme_lower = ascii_lower(parsed.scheme);
    if (scheme_lower != "http" && scheme_lower != "https") {
        return input_url;
    }

    const std::string host_lower = ascii_lower(parsed.host);

    if (looks_like_duckduckgo_host(host_lower)) {
        parsed.scheme = "https";
        parsed.host = "lite.duckduckgo.com";
        if (parsed.path.empty() || parsed.path == "/" ||
            parsed.path == "/index" || parsed.path == "/index.html") {
            parsed.path = "/lite/";
        } else if (parsed.path != "/lite" && parsed.path != "/lite/") {
            parsed.path = "/lite/";
        }
        return parsed.serialize();
    }

    if (looks_like_google_host(host_lower) &&
        is_google_search_compat_path(parsed.path)) {
        if (parsed.path.empty()) {
            parsed.path = "/";
        }
        ensure_query_param(parsed.query, "gbv", "1");
        return parsed.serialize();
    }

    return input_url;
}

static std::string resolve_url_reference(const std::string& href, const std::string& base_url) {
    if (href.empty()) return "";

    // Preserve absolute references exactly as provided.
    if (std::isalpha(static_cast<unsigned char>(href[0]))) {
        size_t i = 1;
        while (i < href.size()) {
            const char c = href[i];
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '-' || c == '.') {
                ++i;
                continue;
            }
            break;
        }
        if (i < href.size() && href[i] == ':') return href;
    }

    // Standards-based resolution path first.
    if (auto base = clever::url::parse(base_url); base.has_value()) {
        if (auto resolved = clever::url::parse(href, &*base); resolved.has_value()) {
            return resolved->serialize();
        }
    }

    // Legacy fallback for non-standard/synthetic base URLs.
    if (base_url.empty()) return href;

    if (href[0] == '?') {
        std::string base = base_url;
        auto hash_pos = base.find('#');
        if (hash_pos != std::string::npos) base.erase(hash_pos);
        auto query_pos = base.find('?');
        if (query_pos != std::string::npos) base.erase(query_pos);
        return base + href;
    }

    if (href[0] == '#') {
        std::string base = base_url;
        auto hash_pos = base.find('#');
        if (hash_pos != std::string::npos) base.erase(hash_pos);
        return base + href;
    }

    if (href.size() >= 2 && href[0] == '/' && href[1] == '/') {
        auto scheme_end = base_url.find("://");
        if (scheme_end != std::string::npos) {
            return base_url.substr(0, scheme_end + 1) + href;
        }
        return "http:" + href;
    }

    if (href[0] == '/') {
        auto scheme_end = base_url.find("://");
        if (scheme_end == std::string::npos) return href;
        auto host_end = base_url.find('/', scheme_end + 3);
        if (host_end == std::string::npos) return base_url + href;
        return base_url.substr(0, host_end) + href;
    }

    auto last_slash = base_url.rfind('/');
    auto scheme_end = base_url.find("://");
    if (scheme_end != std::string::npos && last_slash <= scheme_end + 2) {
        return base_url + "/" + href;
    }
    if (last_slash != std::string::npos) {
        return base_url.substr(0, last_slash + 1) + href;
    }
    return href;
}

static void apply_browser_request_headers(clever::net::Request& req,
                                          const char* accept_value) {
    req.headers.set("User-Agent", kBrowserUserAgent);
    req.headers.set("Accept", accept_value);
    req.headers.set("Accept-Encoding", "gzip, deflate");
}

static std::string escape_html_text(const std::string& input) {
    std::string escaped;
    escaped.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped.push_back(ch); break;
        }
    }
    return escaped;
}

static std::string build_shell_message_html(const std::string& page_title,
                                            const std::string& heading,
                                            const std::string& detail) {
    const std::string safe_title = escape_html_text(page_title);
    const std::string safe_heading = escape_html_text(heading);
    const std::string safe_detail = escape_html_text(detail);
    return "<!doctype html><html lang=\"en\"><head>"
           "<meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
           "<title>" + safe_title + "</title>"
           "<style>"
           ":root{color-scheme:light dark;}"
           "*{box-sizing:border-box;}"
           "html,body{margin:0;padding:0;}"
           "body{font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",sans-serif;"
           "line-height:1.5;background:#f3f6fb;color:#263445;display:flex;"
           "align-items:center;justify-content:center;min-height:100vh;padding:24px;}"
           ".card{width:min(680px,100%);background:#ffffff;border:1px solid #d8e1ec;"
           "border-radius:12px;padding:20px 22px;"
           "box-shadow:0 8px 24px rgba(31,47,70,0.12);}"
           "h1{margin:0 0 10px;font-size:24px;line-height:1.25;color:#1f2b3a;}"
           "p{margin:0;color:#4a5a6e;overflow-wrap:anywhere;}"
           "@media (max-width:640px){body{padding:16px;}h1{font-size:21px;}.card{padding:16px;}}"
           "</style></head><body><main class=\"card\"><h1>" + safe_heading +
           "</h1><p>" + safe_detail + "</p></main></body></html>";
}

// =============================================================================
// BrowserTab — per-tab state
// =============================================================================

@implementation BrowserTab {
    std::string _currentHTML;
    std::string _currentBaseURL;
    std::unordered_map<std::string, float> _idPositions;
    // Persisted JS engine and DOM tree for interactive event dispatch.
    // Owned here so they survive across render_view click events.
    std::unique_ptr<clever::js::JSEngine> _jsEngine;
    std::unique_ptr<clever::html::SimpleNode> _domTree;
    std::vector<clever::paint::ElementRegion> _elementRegions;
    // Layout tree root (kept for CSS transition property access)
    std::unique_ptr<clever::layout::LayoutNode> _layoutRoot;
    // Currently focused text input/textarea DOM node (for inline editing)
    clever::html::SimpleNode* _focusedInputNode;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super init];
    if (self) {
        _renderView = [[RenderView alloc] initWithFrame:frame];
        _renderView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _history = [NSMutableArray new];
        _historyIndex = -1;
        _title = @"New Tab";
        _currentURL = @"";
    }
    return self;
}

- (std::string&)currentHTML { return _currentHTML; }
- (std::string&)currentBaseURL { return _currentBaseURL; }
- (std::unordered_map<std::string, float>&)idPositions { return _idPositions; }
- (std::vector<clever::paint::ElementRegion>&)elementRegions { return _elementRegions; }
- (std::unique_ptr<clever::js::JSEngine>&)jsEngine { return _jsEngine; }
- (std::unique_ptr<clever::html::SimpleNode>&)domTree { return _domTree; }
- (std::unique_ptr<clever::layout::LayoutNode>&)layoutRoot { return _layoutRoot; }
- (clever::html::SimpleNode*)focusedInputNode { return _focusedInputNode; }
- (void)setFocusedInputNode:(clever::html::SimpleNode*)node { _focusedInputNode = node; }

@end

// =============================================================================
// BrowserWindowController — multi-tab browser window
// =============================================================================

@implementation BrowserWindowController {
    NSMutableArray<BrowserTab*>* _tabs;
    NSInteger _activeTabIndex;
    NSView* _toolbarView;
    NSView* _tabBarContainer;
    NSView* _contentArea;
    NSView* _findBar;
    NSTextField* _findField;
    NSTextField* _findStatus;
    BOOL _findBarVisible;
    NSTimer* _resizeTimer;
    NSTimer* _statusBarHideTimer;
    NSTimer* _metaRefreshTimer;
    NSTimer* _progressTimer;
    CGFloat _progressValue; // 0.0 to 1.0
    NSButton* _goButton;
    NSButton* _openAddressDialogButton;
    std::set<int> _toggledDetails; // IDs of <details> elements interactively toggled
    NSMutableArray<NSDictionary*>* _bookmarks;
    // CSS :hover support
    NSTimer* _hoverTimer;
    clever::html::SimpleNode* _hoveredNode; // currently hovered DOM node
    float _lastHoverX, _lastHoverY; // last hover coordinates (buffer pixels)
    BOOL _pageUsesHoverState;
}

- (instancetype)init {
    NSRect frame = NSMakeRect(100, 100, 1024, 768);
    NSUInteger styleMask = NSWindowStyleMaskTitled |
                           NSWindowStyleMaskClosable |
                           NSWindowStyleMaskResizable |
                           NSWindowStyleMaskMiniaturizable |
                           NSWindowStyleMaskFullSizeContentView;

    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                  styleMask:styleMask
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
    self = [super initWithWindow:window];
    if (self) {
        _tabs = [NSMutableArray new];
        _activeTabIndex = -1;
        _pageUsesHoverState = NO;
        _bookmarks = [NSMutableArray new];
        [self loadBookmarks];
        window.titleVisibility = NSWindowTitleHidden;
        window.titlebarAppearsTransparent = YES;
#if defined(MAC_OS_X_VERSION_11_0)
        if (@available(macOS 11.0, *)) {
            window.toolbarStyle = NSWindowToolbarStyleUnifiedCompact;
        }
#endif
        [window setTitle:kBrowserAppName];
        [window setMinSize:NSMakeSize(400, 300)];
        [self setupUI];

        // Open on the built-in display (MacBook screen)
        NSScreen* targetScreen = nil;
        for (NSScreen* screen in [NSScreen screens]) {
            NSNumber* screenNum = [screen deviceDescription][@"NSScreenNumber"];
            CGDirectDisplayID displayID = [screenNum unsignedIntValue];
            if (CGDisplayIsBuiltin(displayID)) {
                targetScreen = screen;
                break;
            }
        }
        if (!targetScreen) targetScreen = [NSScreen mainScreen];

        // Center on the target screen
        NSRect screenFrame = targetScreen.visibleFrame;
        NSRect winFrame = window.frame;
        CGFloat x = screenFrame.origin.x + (screenFrame.size.width - winFrame.size.width) / 2;
        CGFloat y = screenFrame.origin.y + (screenFrame.size.height - winFrame.size.height) / 2;
        [window setFrameOrigin:NSMakePoint(x, y)];

        // Re-render on window resize
        [[NSNotificationCenter defaultCenter] addObserver:self
            selector:@selector(windowDidResize:)
            name:NSWindowDidResizeNotification
            object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self
            selector:@selector(windowDidChangeBackingProperties:)
            name:NSWindowDidChangeBackingPropertiesNotification
            object:window];

        // Create first tab
        [self newTab];
    }
    return self;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)layoutChromeViews {
    NSView* contentView = self.window.contentView;
    if (!contentView || !_toolbarView || !_tabBarContainer || !_contentArea) return;

    NSRect bounds = contentView.bounds;
    const CGFloat width = bounds.size.width;
    const CGFloat height = bounds.size.height;
    const CGFloat chromeHeight = kTabBarHeight + kToolbarHeight;
    const CGFloat contentHeight = std::max<CGFloat>(1.0, std::floor(height - chromeHeight));
    const CGFloat toolbarY = contentHeight;
    const CGFloat tabY = contentHeight + kToolbarHeight;

    _tabBarContainer.frame = NSMakeRect(0, tabY, width, kTabBarHeight);
    _toolbarView.frame = NSMakeRect(0, toolbarY, width, kToolbarHeight);
    _contentArea.frame = NSMakeRect(0, 0, width, contentHeight);

    CGFloat tabLeading = kChromeHorizontalInset;
    NSButton* closeButton = [self.window standardWindowButton:NSWindowCloseButton];
    if (closeButton && closeButton.superview) {
        NSRect closeFrame = [_tabBarContainer convertRect:closeButton.frame fromView:closeButton.superview];
        tabLeading = std::max(tabLeading, NSMaxX(closeFrame) + kTrafficLightClearance);
    }
    const CGFloat tabTrailingReserve = kChromeHorizontalInset + kTabPlusButtonWidth + 8.0;
    if (_tabBarScrollView) {
        NSRect scrollFrame = _tabBarScrollView.frame;
        scrollFrame.origin.x = tabLeading;
        scrollFrame.size.width = std::max<CGFloat>(0.0,
            width - tabLeading - tabTrailingReserve);
        _tabBarScrollView.frame = scrollFrame;
    }

    const CGFloat toolbarLeft =
        kChromeHorizontalInset + (kToolbarButtonWidth * 4.0) + (kToolbarControlGap * 3.0) + 10.0;
    const CGFloat rightClusterWidth =
        (kToolbarActionButtonWidth * 2.0) + kToolbarActionButtonGap;
    const CGFloat rightButtonX = width - kChromeHorizontalInset - rightClusterWidth;
    const CGFloat buttonY = std::floor((kToolbarHeight - kToolbarButtonHeight) / 2.0);

    if (_goButton) {
        _goButton.frame = NSMakeRect(rightButtonX, buttonY, kToolbarActionButtonWidth, kToolbarButtonHeight);
    }
    if (_openAddressDialogButton) {
        _openAddressDialogButton.frame = NSMakeRect(
            rightButtonX + kToolbarActionButtonWidth + kToolbarActionButtonGap,
            buttonY,
            kToolbarActionButtonWidth,
            kToolbarButtonHeight);
    }
    if (_spinner) {
        const CGFloat spinnerSize = 18.0;
        const CGFloat spinnerX = rightButtonX - kToolbarControlGap - spinnerSize;
        _spinner.frame = NSMakeRect(
            spinnerX,
            std::floor((kToolbarHeight - spinnerSize) / 2.0),
            spinnerSize,
            spinnerSize);
    }
    if (_addressBar) {
        CGFloat addressBarWidth = rightButtonX - toolbarLeft - kToolbarControlGap;
        if (addressBarWidth < kAddressBarMinWidth) {
            addressBarWidth = kAddressBarMinWidth;
        }
        _addressBar.frame = NSMakeRect(
            toolbarLeft,
            std::floor((kToolbarHeight - kAddressBarHeight) / 2.0),
            addressBarWidth,
            kAddressBarHeight);
    }
}

- (void)setupUI {
    NSView* contentView = self.window.contentView;
    if (!contentView) return;
    contentView.wantsLayer = YES;
    contentView.layer.backgroundColor = [NSColor windowBackgroundColor].CGColor;

    // --- Tab strip ---
    NSVisualEffectView* tabBarEffect = [[NSVisualEffectView alloc] initWithFrame:NSMakeRect(
        0, contentView.bounds.size.height - kTabBarHeight, contentView.bounds.size.width, kTabBarHeight)];
    tabBarEffect.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    tabBarEffect.material = NSVisualEffectMaterialHeaderView;
    tabBarEffect.blendingMode = NSVisualEffectBlendingModeWithinWindow;
    tabBarEffect.state = NSVisualEffectStateFollowsWindowActiveState;
    _tabBarContainer = tabBarEffect;

    _tabBarScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(
        kChromeHorizontalInset, 0,
        std::max<CGFloat>(0.0, contentView.bounds.size.width - (kChromeHorizontalInset * 2.0 + kTabPlusButtonWidth + 8.0)),
        kTabBarHeight)];
    _tabBarScrollView.autoresizingMask = NSViewWidthSizable;
    _tabBarScrollView.hasHorizontalScroller = YES;
    _tabBarScrollView.hasVerticalScroller = NO;
    _tabBarScrollView.autohidesScrollers = YES;
    _tabBarScrollView.borderType = NSNoBorder;
    _tabBarScrollView.drawsBackground = NO;
    _tabBarScrollView.scrollerStyle = NSScrollerStyleOverlay;

    _tabBar = [[NSStackView alloc] initWithFrame:NSMakeRect(0, 0, 240, kTabBarHeight)];
    _tabBar.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    _tabBar.spacing = 4;
    _tabBar.alignment = NSLayoutAttributeCenterY;
    _tabBar.edgeInsets = NSEdgeInsetsMake(5, 8, 5, 4);
    _tabBar.detachesHiddenViews = YES;
    _tabBar.autoresizingMask = NSViewNotSizable;
    _tabBarScrollView.documentView = _tabBar;
    [_tabBarContainer addSubview:_tabBarScrollView];

    NSButton* newTabBtn = [NSButton buttonWithTitle:@""
                                             target:self
                                             action:@selector(newTabAction:)];
    newTabBtn.frame = NSMakeRect(contentView.bounds.size.width - kChromeHorizontalInset - kTabPlusButtonWidth,
                                 (kTabBarHeight - 26.0) / 2.0, kTabPlusButtonWidth, 26.0);
    newTabBtn.bezelStyle = NSBezelStyleAccessoryBarAction;
    newTabBtn.controlSize = NSControlSizeSmall;
    newTabBtn.image = browser_symbol_image(@"plus");
    newTabBtn.imagePosition = NSImageOnly;
    if (!newTabBtn.image) {
        newTabBtn.title = @"+";
        newTabBtn.imagePosition = NSNoImage;
    }
    newTabBtn.toolTip = @"New tab";
    newTabBtn.autoresizingMask = NSViewMinXMargin;
    [_tabBarContainer addSubview:newTabBtn];

    NSView* tabSeparator = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, contentView.bounds.size.width, 1)];
    tabSeparator.wantsLayer = YES;
    tabSeparator.layer.backgroundColor = [[NSColor separatorColor] colorWithAlphaComponent:0.5].CGColor;
    tabSeparator.autoresizingMask = NSViewWidthSizable;
    [_tabBarContainer addSubview:tabSeparator];

    [contentView addSubview:_tabBarContainer];

    // --- Toolbar ---
    NSVisualEffectView* toolbarEffect = [[NSVisualEffectView alloc] initWithFrame:NSMakeRect(0,
        contentView.bounds.size.height - kTabBarHeight - kToolbarHeight,
        contentView.bounds.size.width, kToolbarHeight)];
    toolbarEffect.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    toolbarEffect.material = NSVisualEffectMaterialWindowBackground;
    toolbarEffect.blendingMode = NSVisualEffectBlendingModeWithinWindow;
    toolbarEffect.state = NSVisualEffectStateFollowsWindowActiveState;
    _toolbarView = toolbarEffect;

    CGFloat x = kChromeHorizontalInset;
    CGFloat btnY = (kToolbarHeight - kToolbarButtonHeight) / 2.0;

    _backButton = [NSButton buttonWithTitle:@""
                                     target:self
                                     action:@selector(goBack:)];
    _backButton.frame = NSMakeRect(x, btnY, kToolbarButtonWidth, kToolbarButtonHeight);
    _backButton.bezelStyle = NSBezelStyleAccessoryBarAction;
    _backButton.controlSize = NSControlSizeRegular;
    _backButton.image = browser_symbol_image(@"chevron.left");
    _backButton.imagePosition = NSImageOnly;
    if (!_backButton.image) {
        _backButton.title = @"\u276E";
        _backButton.imagePosition = NSNoImage;
    }
    _backButton.toolTip = @"Back";
    _backButton.enabled = NO;
    [_toolbarView addSubview:_backButton];
    x += kToolbarButtonWidth + kToolbarControlGap;

    _forwardButton = [NSButton buttonWithTitle:@""
                                        target:self
                                        action:@selector(goForward:)];
    _forwardButton.frame = NSMakeRect(x, btnY, kToolbarButtonWidth, kToolbarButtonHeight);
    _forwardButton.bezelStyle = NSBezelStyleAccessoryBarAction;
    _forwardButton.controlSize = NSControlSizeRegular;
    _forwardButton.image = browser_symbol_image(@"chevron.right");
    _forwardButton.imagePosition = NSImageOnly;
    if (!_forwardButton.image) {
        _forwardButton.title = @"\u276F";
        _forwardButton.imagePosition = NSNoImage;
    }
    _forwardButton.toolTip = @"Forward";
    _forwardButton.enabled = NO;
    [_toolbarView addSubview:_forwardButton];
    x += kToolbarButtonWidth + kToolbarControlGap;

    _reloadButton = [NSButton buttonWithTitle:@""
                                       target:self
                                       action:@selector(reload:)];
    _reloadButton.frame = NSMakeRect(x, btnY, kToolbarButtonWidth, kToolbarButtonHeight);
    _reloadButton.bezelStyle = NSBezelStyleAccessoryBarAction;
    _reloadButton.controlSize = NSControlSizeRegular;
    _reloadButton.image = browser_symbol_image(@"arrow.clockwise");
    _reloadButton.imagePosition = NSImageOnly;
    if (!_reloadButton.image) {
        _reloadButton.title = @"\u21BB";
        _reloadButton.imagePosition = NSNoImage;
    }
    _reloadButton.toolTip = @"Reload";
    [_toolbarView addSubview:_reloadButton];
    x += kToolbarButtonWidth + kToolbarControlGap;

    _homeButton = [NSButton buttonWithTitle:@""
                                     target:self
                                     action:@selector(goHome:)];
    _homeButton.frame = NSMakeRect(x, btnY, kToolbarButtonWidth, kToolbarButtonHeight);
    _homeButton.bezelStyle = NSBezelStyleAccessoryBarAction;
    _homeButton.controlSize = NSControlSizeRegular;
    _homeButton.image = browser_symbol_image(@"house");
    _homeButton.imagePosition = NSImageOnly;
    if (!_homeButton.image) {
        _homeButton.title = @"\u2302";
        _homeButton.imagePosition = NSNoImage;
    }
    _homeButton.toolTip = @"Home";
    [_toolbarView addSubview:_homeButton];
    x += kToolbarButtonWidth + 10.0;

    _spinner = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(
        contentView.bounds.size.width - kChromeHorizontalInset - 20.0,
        std::floor((kToolbarHeight - 18.0) / 2.0), 18.0, 18.0)];
    _spinner.style = NSProgressIndicatorStyleSpinning;
    _spinner.displayedWhenStopped = NO;
    _spinner.controlSize = NSControlSizeSmall;
    _spinner.autoresizingMask = NSViewMinXMargin;
    [_toolbarView addSubview:_spinner];

    CGFloat rightBtnX = contentView.bounds.size.width - kChromeHorizontalInset -
        (kToolbarActionButtonWidth * 2.0 + kToolbarActionButtonGap);

    _goButton = [NSButton buttonWithTitle:@""
                                   target:self
                                   action:@selector(addressBarSubmitted:)];
    _goButton.frame = NSMakeRect(rightBtnX, btnY, kToolbarActionButtonWidth, kToolbarButtonHeight);
    _goButton.bezelStyle = NSBezelStyleAccessoryBarAction;
    _goButton.controlSize = NSControlSizeRegular;
    _goButton.image = browser_symbol_image(@"arrow.right.circle.fill");
    _goButton.imagePosition = NSImageOnly;
    if (!_goButton.image) {
        _goButton.title = @"\u2192";
        _goButton.imagePosition = NSNoImage;
    }
    _goButton.autoresizingMask = NSViewMinXMargin;
    _goButton.toolTip = @"Open URL";
    [_toolbarView addSubview:_goButton];

    _openAddressDialogButton = [NSButton buttonWithTitle:@""
                                                  target:self
                                                  action:@selector(openAddressBarModal)];
    _openAddressDialogButton.frame = NSMakeRect(
        rightBtnX + kToolbarActionButtonWidth + kToolbarActionButtonGap,
        btnY,
        kToolbarActionButtonWidth,
        kToolbarButtonHeight);
    _openAddressDialogButton.bezelStyle = NSBezelStyleAccessoryBarAction;
    _openAddressDialogButton.controlSize = NSControlSizeRegular;
    _openAddressDialogButton.image = browser_symbol_image(@"ellipsis.circle");
    _openAddressDialogButton.imagePosition = NSImageOnly;
    if (!_openAddressDialogButton.image) {
        _openAddressDialogButton.title = @"\u2026";
        _openAddressDialogButton.imagePosition = NSNoImage;
    }
    _openAddressDialogButton.autoresizingMask = NSViewMinXMargin;
    _openAddressDialogButton.toolTip = @"Open URL in dialog";
    [_toolbarView addSubview:_openAddressDialogButton];

    const CGFloat spinnerSize = 18.0;
    _spinner.frame = NSMakeRect(
        rightBtnX - kToolbarControlGap - spinnerSize,
        std::floor((kToolbarHeight - spinnerSize) / 2.0),
        spinnerSize,
        spinnerSize);

    CGFloat addressBarWidth = rightBtnX - x - kToolbarControlGap;
    if (addressBarWidth < kAddressBarMinWidth) addressBarWidth = kAddressBarMinWidth;
    _addressBar = [[AddressBarTextField alloc] initWithFrame:NSMakeRect(
        x, std::floor((kToolbarHeight - kAddressBarHeight) / 2.0), addressBarWidth, kAddressBarHeight)];
    _addressBar.placeholderString = @"Search or enter URL...";
    _addressBar.font = [NSFont systemFontOfSize:14 weight:NSFontWeightRegular];
    _addressBar.editable = YES;
    _addressBar.selectable = YES;
    _addressBar.bordered = NO;
    _addressBar.bezeled = YES;
    _addressBar.bezelStyle = NSTextFieldRoundedBezel;
    _addressBar.target = self;
    _addressBar.action = @selector(addressBarSubmitted:);
    _addressBar.delegate = self;
    _addressBar.drawsBackground = YES;
    _addressBar.backgroundColor = [NSColor controlBackgroundColor];
    _addressBar.autoresizingMask = NSViewWidthSizable;
    _addressBar.focusRingType = NSFocusRingTypeExterior;
    _addressBar.wantsLayer = YES;
    _addressBar.layer.cornerRadius = 11.0;
    _addressBar.layer.borderWidth = 1.0;
    _addressBar.layer.borderColor = [[NSColor separatorColor] colorWithAlphaComponent:0.25].CGColor;
    _addressBar.layer.masksToBounds = YES;
    [_toolbarView addSubview:_addressBar];

    NSView* toolbarBorder = [[NSView alloc] initWithFrame:NSMakeRect(0, 0,
        _toolbarView.bounds.size.width, 1.0)];
    toolbarBorder.wantsLayer = YES;
    toolbarBorder.layer.backgroundColor = [[NSColor separatorColor] colorWithAlphaComponent:0.5].CGColor;
    toolbarBorder.autoresizingMask = NSViewWidthSizable;
    [_toolbarView addSubview:toolbarBorder];

    [self.window setInitialFirstResponder:_addressBar];

    [contentView addSubview:_toolbarView];

    // --- Web content area ---
    _contentArea = [[NSView alloc] initWithFrame:NSMakeRect(0, 0,
        contentView.bounds.size.width,
        contentView.bounds.size.height - kToolbarHeight - kTabBarHeight)];
    _contentArea.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [contentView addSubview:_contentArea];
    [self layoutChromeViews];

    static const CGFloat kProgressBarHeight = 3.0;
    _progressBar = [[NSView alloc] initWithFrame:NSMakeRect(0,
        _contentArea.bounds.size.height - kProgressBarHeight,
        0, kProgressBarHeight)];
    _progressBar.wantsLayer = YES;
    _progressBar.layer.backgroundColor = [[NSColor controlAccentColor] CGColor];
    _progressBar.layer.cornerRadius = 1.5;
    _progressBar.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    _progressBar.hidden = YES;
    [_contentArea addSubview:_progressBar positioned:NSWindowAbove relativeTo:nil];

    static const CGFloat kStatusBarHeight = 20.0;
    _statusBar = [NSTextField labelWithString:@""];
    _statusBar.frame = NSMakeRect(0, 0,
        _contentArea.bounds.size.width * 0.6, kStatusBarHeight);
    _statusBar.font = [NSFont systemFontOfSize:11 weight:NSFontWeightRegular];
    _statusBar.textColor = [NSColor secondaryLabelColor];
    _statusBar.backgroundColor = [NSColor colorWithCalibratedWhite:0.96 alpha:0.95];
    _statusBar.drawsBackground = YES;
    _statusBar.bordered = NO;
    _statusBar.editable = NO;
    _statusBar.selectable = NO;
    _statusBar.lineBreakMode = NSLineBreakByTruncatingMiddle;
    _statusBar.wantsLayer = YES;
    _statusBar.layer.cornerRadius = 6.0;
    _statusBar.layer.borderWidth = 0.5;
    _statusBar.layer.borderColor = [[NSColor separatorColor] colorWithAlphaComponent:0.3].CGColor;
    _statusBar.autoresizingMask = NSViewMaxXMargin | NSViewMaxYMargin;
    // Add left padding by using an attributed string approach via cell inset
    [[_statusBar cell] setWraps:NO];
    [[_statusBar cell] setScrollable:NO];
    _statusBar.hidden = YES;
    _statusBar.alphaValue = 0.0;
    [_contentArea addSubview:_statusBar positioned:NSWindowAbove relativeTo:nil];
}

- (void)focusAddressBarAndSelectAll {
    if (!self.window || !_addressBar) return;
    [self.window makeKeyAndOrderFront:nil];
    void (^focusBlock)(void) = ^{
        BrowserTab* tab = [self activeTab];
        [tab.renderView dismissTextInputOverlay];
        _addressBar.enabled = YES;
        _addressBar.editable = YES;
        _addressBar.selectable = YES;
        BOOL becameFirstResponder = [self.window makeFirstResponder:_addressBar];
        if (!becameFirstResponder) {
            [self openAddressBarModal];
            return;
        }
        NSText* editor = [self.window fieldEditor:YES forObject:_addressBar];
        if (editor && [editor respondsToSelector:@selector(setSelectedRange:)]) {
            [editor setSelectedRange:NSMakeRange([[editor string] length], 0)];
        }
        [_addressBar selectText:nil];
    };

    if ([NSThread isMainThread]) {
        focusBlock();
    } else {
        dispatch_async(dispatch_get_main_queue(), focusBlock);
    }
}

#pragma mark - Tab Management

- (BrowserTab*)activeTab {
    if (_activeTabIndex >= 0 && _activeTabIndex < (NSInteger)_tabs.count) {
        return _tabs[_activeTabIndex];
    }
    return nil;
}

- (void)newTab {
    NSRect frame = _contentArea.bounds;
    if (frame.size.width < 1) frame = NSMakeRect(0, 0, 800, 600);

    BrowserTab* tab = [[BrowserTab alloc] initWithFrame:frame];
    tab.renderView.delegate = self;
    [_tabs addObject:tab];
    [self switchToTab:_tabs.count - 1];
    [self rebuildTabBar];
}

- (void)newTabAction:(id)sender {
    (void)sender;
    [self newTab];
}

- (void)closeCurrentTab {
    if (_tabs.count <= 1) return;  // Don't close last tab

    // Remove current tab's view
    BrowserTab* tab = [self activeTab];
    [tab.renderView removeFromSuperview];

    [_tabs removeObjectAtIndex:_activeTabIndex];

    if (_activeTabIndex >= (NSInteger)_tabs.count) {
        _activeTabIndex = _tabs.count - 1;
    }

    [self switchToTab:_activeTabIndex];
    [self rebuildTabBar];
}

- (void)switchToTabByNumber:(NSInteger)number {
    // Cmd+1 through Cmd+8: switch to tab at that index (1-based)
    // Cmd+9: always switch to the last tab
    if (_tabs.count == 0) return;

    NSInteger index;
    if (number == 9) {
        index = _tabs.count - 1;
    } else {
        index = number - 1;  // Convert 1-based to 0-based
    }

    if (index >= 0 && index < (NSInteger)_tabs.count) {
        [self switchToTab:index];
    }
}

- (void)nextTab {
    if (_tabs.count <= 1) return;
    NSInteger next = (_activeTabIndex + 1) % _tabs.count;
    [self switchToTab:next];
}

- (void)previousTab {
    if (_tabs.count <= 1) return;
    NSInteger prev = (_activeTabIndex - 1 + _tabs.count) % _tabs.count;
    [self switchToTab:prev];
}

- (void)switchToTab:(NSInteger)index {
    if (index < 0 || index >= (NSInteger)_tabs.count) return;

    // Remove current tab's render view
    BrowserTab* currentTab = [self activeTab];
    if (currentTab) {
        [currentTab.renderView removeFromSuperview];
    }

    _activeTabIndex = index;
    BrowserTab* newTab = _tabs[index];

    // Add new tab's render view below the progress bar and status bar overlays
    newTab.renderView.frame = _contentArea.bounds;
    [_contentArea addSubview:newTab.renderView positioned:NSWindowBelow relativeTo:_progressBar];

    // Update address bar
    if (newTab.currentURL.length > 0) {
        [_addressBar setStringValue:newTab.currentURL];
    } else {
        [_addressBar setStringValue:@""];
    }

    // Update window title
    if (newTab.title.length > 0 && ![newTab.title isEqualToString:@"New Tab"]) {
        self.window.title = [NSString stringWithFormat:@"%@ - %@", newTab.title, kBrowserAppName];
    } else {
        self.window.title = kBrowserAppName;
    }

    [self updateNavButtons];
    [self rebuildTabBar];
}

- (void)tabClicked:(NSButton*)sender {
    NSInteger index = sender.tag;
    [self switchToTab:index];
}

- (void)closeTabClicked:(NSButton*)sender {
    NSInteger index = sender.tag;
    if (_tabs.count <= 1) return;

    if (index == _activeTabIndex) {
        [self closeCurrentTab];
    } else {
        BrowserTab* tab = _tabs[index];
        [tab.renderView removeFromSuperview];
        [_tabs removeObjectAtIndex:index];

        if (_activeTabIndex > index) {
            _activeTabIndex--;
        }
        [self rebuildTabBar];
    }
}

- (void)rebuildTabBar {
    if (!_tabBar) return;
    // Remove all existing tab buttons
    for (NSView* view in [_tabBar.arrangedSubviews copy]) {
        [_tabBar removeArrangedSubview:view];
        [view removeFromSuperview];
    }

    // Create a button for each tab
    for (NSInteger i = 0; i < (NSInteger)_tabs.count; i++) {
        BrowserTab* tab = _tabs[i];
        const CGFloat tabViewHeight = std::max<CGFloat>(24.0, kTabBarHeight - 6.0);
        const CGFloat closeAreaWidth = (_tabs.count > 1) ? 20.0 : 0.0;

        NSView* tabView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 180, tabViewHeight)];

        // Favicon image view (16x16, left side of tab)
        CGFloat faviconOffset = 0;
        if (tab.faviconImage) {
            const CGFloat faviconY = std::floor((tabViewHeight - 16.0) / 2.0);
            NSImageView* faviconView = [[NSImageView alloc] initWithFrame:NSMakeRect(6, faviconY, 16, 16)];
            faviconView.image = tab.faviconImage;
            faviconView.imageScaling = NSImageScaleProportionallyUpOrDown;
            [tabView addSubview:faviconView];
            faviconOffset = 24; // 16px icon + 8px gap
        }

        // Tab title button
        NSString* title = normalized_tab_title(tab.title);
        NSInteger maxLen = tab.faviconImage ? 16 : 20;
        if ((NSInteger)title.length > maxLen) {
            title = [[title substringToIndex:maxLen - 3] stringByAppendingString:@"..."];
        }

        NSButton* tabBtn = [NSButton buttonWithTitle:title
                                              target:self
                                              action:@selector(tabClicked:)];
        tabBtn.tag = i;
        const CGFloat tabButtonWidth = std::max<CGFloat>(80.0, 176.0 - faviconOffset - closeAreaWidth);
        tabBtn.frame = NSMakeRect(faviconOffset, 0, tabButtonWidth, tabViewHeight);
        tabBtn.buttonType = NSButtonTypeToggle;
        tabBtn.bezelStyle = NSBezelStyleRecessed;
        tabBtn.controlSize = NSControlSizeSmall;
        tabBtn.font = [NSFont systemFontOfSize:11 weight:NSFontWeightMedium];
        tabBtn.imagePosition = NSNoImage;

        if (i == _activeTabIndex) {
            tabBtn.state = NSControlStateValueOn;
        }

        [tabView addSubview:tabBtn];

        // Close button (x)
        if (_tabs.count > 1) {
            NSButton* closeBtn = [NSButton buttonWithTitle:@""
                                                    target:self
                                                    action:@selector(closeTabClicked:)];
            closeBtn.tag = i;
            const CGFloat closeSize = 16.0;
            const CGFloat closeY = std::floor((tabViewHeight - closeSize) / 2.0);
            closeBtn.frame = NSMakeRect(160, closeY, 18, closeSize);
            closeBtn.bezelStyle = NSBezelStyleCircular;
            closeBtn.controlSize = NSControlSizeMini;
            closeBtn.image = browser_symbol_image(@"xmark");
            closeBtn.imagePosition = NSImageOnly;
            if (!closeBtn.image) {
                closeBtn.title = @"x";
                closeBtn.imagePosition = NSNoImage;
            }
            closeBtn.toolTip = @"Close tab";
            [tabView addSubview:closeBtn];
        }

        [_tabBar addArrangedSubview:tabView];
    }

    CGFloat totalWidth = 8.0;
    NSArray<NSView*>* arranged = _tabBar.arrangedSubviews;
    for (NSView* view in arranged) {
        totalWidth += NSWidth(view.frame);
    }
    if (arranged.count > 1) {
        totalWidth += _tabBar.spacing * (arranged.count - 1);
    }
    CGFloat viewportWidth = _tabBarScrollView ? _tabBarScrollView.contentSize.width : totalWidth;
    CGFloat finalWidth = std::max(totalWidth, viewportWidth);
    _tabBar.frame = NSMakeRect(0, 0, finalWidth, kTabBarHeight);

    if (_activeTabIndex >= 0 && _activeTabIndex < (NSInteger)arranged.count) {
        NSView* activeView = arranged[_activeTabIndex];
        [_tabBar scrollRectToVisible:NSInsetRect(activeView.frame, -24.0, 0.0)];
    }
}

#pragma mark - Progress Bar

- (void)setProgress:(CGFloat)value animated:(BOOL)animated {
    _progressValue = value;
    CGFloat barWidth = _contentArea.bounds.size.width * value;
    CGFloat barY = _contentArea.bounds.size.height - 3.0;

    if (value <= 0) {
        // Hide immediately
        [_progressTimer invalidate];
        _progressTimer = nil;
        _progressBar.hidden = YES;
        _progressBar.frame = NSMakeRect(0, barY, 0, 3.0);
        return;
    }

    _progressBar.hidden = NO;

    if (animated) {
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext* ctx) {
            ctx.duration = 0.3;
            ctx.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
            [[self.progressBar animator] setFrame:NSMakeRect(0, barY, barWidth, 3.0)];
        }];
    } else {
        _progressBar.frame = NSMakeRect(0, barY, barWidth, 3.0);
    }
}

- (void)startProgressForNavigation {
    // Cancel any previous progress animation
    [_progressTimer invalidate];
    _progressTimer = nil;

    // Reset alpha in case a previous finish animation was in flight
    _progressBar.alphaValue = 1.0;

    // Start at 10%
    [self setProgress:0.1 animated:NO];

    // Gradually animate from 10% toward 90% while waiting for the fetch
    _progressTimer = [NSTimer scheduledTimerWithTimeInterval:0.5
                                                      target:self
                                                    selector:@selector(progressTimerTick:)
                                                    userInfo:nil
                                                     repeats:YES];
}

- (void)progressTimerTick:(NSTimer*)timer {
    (void)timer;
    // Ease toward 90% but never reach it — slows down as it gets closer
    CGFloat remaining = 0.9 - _progressValue;
    CGFloat increment = remaining * 0.15;
    if (increment < 0.005) increment = 0.005;
    CGFloat newValue = _progressValue + increment;
    if (newValue > 0.9) newValue = 0.9;
    [self setProgress:newValue animated:YES];
}

- (void)finishProgress {
    [_progressTimer invalidate];
    _progressTimer = nil;

    // Animate to 100%, then fade out after a short delay
    [self setProgress:1.0 animated:YES];

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.4 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext* ctx) {
            ctx.duration = 0.25;
            self.progressBar.animator.alphaValue = 0.0;
        } completionHandler:^{
            [self setProgress:0.0 animated:NO];
            self.progressBar.alphaValue = 1.0;
        }];
    });
}

#pragma mark - Navigation

- (void)navigateToURL:(NSString*)urlString {
    if (!urlString) return;
    NSString* trimmedURL = [urlString
        stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (trimmedURL.length == 0) return;

    // Cancel any pending meta refresh timer
    [_metaRefreshTimer invalidate];
    _metaRefreshTimer = nil;

    // Clear interactive details toggle state for fresh page
    _toggledDetails.clear();

    BrowserTab* tab = [self activeTab];
    if (!tab) return;

    NSString* url = trimmedURL;

    // Auto-add http:// if no scheme
    if (![url hasPrefix:@"http://"] && ![url hasPrefix:@"https://"] &&
        ![url hasPrefix:@"<"]) {
        if ([url containsString:@"<"] && [url containsString:@">"]) {
            [self renderHTML:url];
            return;
        }
        url = [@"http://" stringByAppendingString:url];
    }

    if ([url hasPrefix:@"<"]) {
        [self renderHTML:url];
        return;
    }

    std::string normalizedUrl = normalize_url_for_compat(std::string([url UTF8String]));
    url = [NSString stringWithUTF8String:normalizedUrl.c_str()];

    // Update address bar and tab state
    [_addressBar setStringValue:url];
    tab.currentURL = url;

    // Add to history (skip duplicate consecutive entries)
    BOOL shouldPushHistory = YES;
    if (tab.historyIndex >= 0 && tab.historyIndex < (NSInteger)tab.history.count) {
        NSString* currentHistoryEntry = tab.history[tab.historyIndex];
        if ([currentHistoryEntry isEqualToString:url]) {
            shouldPushHistory = NO;
        }
    }
    if (shouldPushHistory) {
        while ((NSInteger)tab.history.count > tab.historyIndex + 1) {
            [tab.history removeLastObject];
        }
        [tab.history addObject:url];
        tab.historyIndex = tab.history.count - 1;
    }
    [self updateNavButtons];

    // Fetch in background
    [_spinner startAnimation:nil];
    [self startProgressForNavigation];
    self.window.title = [NSString stringWithFormat:@"Loading... - %@", kBrowserAppName];

    std::string urlStr = std::string([url UTF8String]);

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [self fetchAndRender:urlStr];
    });
}

- (void)fetchAndRender:(const std::string&)urlStr {
    clever::net::HttpClient client;
    client.set_timeout(std::chrono::seconds(10));
    client.set_max_redirects(0);

    std::string currentUrl = normalize_url_for_compat(urlStr);
    std::optional<clever::net::Response> response;
    std::string lastFetchedUrl = currentUrl;
    int redirects = 0;
    const int maxRedirects = 10;

    auto& jar = clever::net::CookieJar::shared();

    while (redirects < maxRedirects) {
        currentUrl = normalize_url_for_compat(currentUrl);
        clever::net::Request req;
        req.url = currentUrl;
        req.method = clever::net::Method::GET;
        req.parse_url();
        apply_browser_request_headers(req, "text/html,application/xhtml+xml,*/*;q=0.8");

        // Attach cookies
        std::string cookies = jar.get_cookie_header(req.host, req.path, req.use_tls);
        if (!cookies.empty()) {
            req.headers.set("Cookie", cookies);
        }

        response = client.fetch(req);

        if (!response) break;
        const std::string responseUrl = response->url.empty() ? currentUrl : response->url;
        lastFetchedUrl = responseUrl;

        // Store cookies from response
        for (auto& cookie_val : response->headers.get_all("set-cookie")) {
            jar.set_from_header(cookie_val, req.host);
        }

        // Follow redirects
        if (response->status == 301 || response->status == 302 ||
            response->status == 303 || response->status == 307 ||
            response->status == 308) {
            auto location = response->headers.get("location");
            if (!location || location->empty()) break;
            currentUrl = resolve_url_reference(*location, responseUrl);
            redirects++;
            continue;
        }
        break;
    }

    std::string finalUrl = lastFetchedUrl;

    dispatch_async(dispatch_get_main_queue(), ^{
        [_spinner stopAnimation:nil];

        BrowserTab* tab = [self activeTab];
        if (!tab) return;

        // Response received — jump progress to 70%
        [self setProgress:0.7 animated:YES];

        if (!response || response->status >= 400) {
            std::string error_html;
            if (!response) {
                error_html = build_shell_message_html(
                    "Connection Failed",
                    "Connection Failed",
                    "Could not connect to " + urlStr);
            } else {
                const std::string status_heading = "Error " + std::to_string(response->status);
                error_html = build_shell_message_html(
                    status_heading,
                    status_heading,
                    response->status_text);
            }
            [self renderHTML:[NSString stringWithUTF8String:error_html.c_str()]];
            [self finishProgress];
            return;
        }

        // Update address bar with final URL after redirects
        if (finalUrl != urlStr) {
            tab.currentURL = [NSString stringWithUTF8String:finalUrl.c_str()];
            [_addressBar setStringValue:tab.currentURL];
        }

        std::string html = response->body_as_string();
        [tab currentHTML] = html;
        [tab currentBaseURL] = finalUrl;

        // Rendering starts — set progress to 90%
        [self setProgress:0.9 animated:YES];

        [self doRender:html];

        // Rendering complete — finish progress bar
        [self finishProgress];
    });
}

- (void)renderHTML:(NSString*)html {
    BrowserTab* tab = [self activeTab];
    if (!tab) return;

    std::string htmlStr([html UTF8String]);
    [tab currentHTML] = htmlStr;
    [tab currentBaseURL] = "";
    [self doRender:htmlStr];
}

- (void)doRender:(const std::string&)html {
    BrowserTab* tab = [self activeTab];
    if (!tab) return;
    _pageUsesHoverState = page_uses_hover_state(html);

    NSRect bounds = tab.renderView.bounds;
    int width = static_cast<int>(std::round(bounds.size.width));
    int height = static_cast<int>(std::round(bounds.size.height));
    if (width < 1) width = 800;
    if (height < 1) height = 600;
    NSScreen* windowScreen = self.window.screen;
    CGFloat scaleFactor = windowScreen ? windowScreen.backingScaleFactor : 1.0;
    if (!std::isfinite(scaleFactor) || scaleFactor <= 0.0) {
        scaleFactor = 1.0;
    }
    int renderWidth = static_cast<int>(std::round(static_cast<CGFloat>(width) * scaleFactor));
    int renderHeight = static_cast<int>(std::round(static_cast<CGFloat>(height) * scaleFactor));
    if (renderWidth < 1) renderWidth = width;
    if (renderHeight < 1) renderHeight = height;

    auto result = _toggledDetails.empty()
        ? clever::paint::render_html(html, [tab currentBaseURL], renderWidth, renderHeight)
        : clever::paint::render_html(html, [tab currentBaseURL], renderWidth, renderHeight, _toggledDetails);

    if (result.success && result.renderer) {
        // Reset scroll position to top on new page load (but not on details toggle)
        if (_toggledDetails.empty()) {
            tab.renderView.scrollOffset = 0;
        }

        auto& oldEngine = [tab jsEngine];
        if (oldEngine && oldEngine->context()) {
            clever::js::cleanup_dom_bindings(oldEngine->context());
            clever::js::cleanup_timers(oldEngine->context());
        }
        oldEngine.reset();
        [tab setFocusedInputNode:nullptr];

        [tab.renderView updateWithRenderer:result.renderer.get()];
        [tab.renderView updateLinks:result.links];
        [tab.renderView updateCursorRegions:result.cursor_regions];
        [tab.renderView updateTextRegions:result.text_commands];
        [tab.renderView updateSelectionColors:result.selection_color bg:result.selection_bg_color];
        [tab.renderView updateFormSubmitRegions:result.form_submit_regions];
        [tab.renderView updateDetailsToggleRegions:result.details_toggle_regions];
        [tab.renderView updateSelectClickRegions:result.select_click_regions];
        [tab.renderView updateFormData:result.forms];
        [tab idPositions] = std::move(result.id_positions);

        // Scroll to URL fragment (#section) on initial page load
        if (_toggledDetails.empty()) {
            NSString* urlStr = tab.currentURL;
            NSRange hashRange = [urlStr rangeOfString:@"#" options:NSBackwardsSearch];
            if (hashRange.location != NSNotFound && hashRange.location < urlStr.length - 1) {
                NSString* frag = [urlStr substringFromIndex:hashRange.location + 1];
                std::string fragStr([frag UTF8String]);
                auto& positions = [tab idPositions];
                auto it = positions.find(fragStr);
                if (it != positions.end()) {
                    tab.renderView.scrollOffset = [tab.renderView viewOffsetForRendererY:it->second];
                }
            }
        }

        // Transfer the persisted JS engine, DOM tree, and element regions
        // from the render result to the tab for interactive event dispatch.
        [tab jsEngine] = std::move(result.js_engine);
        [tab domTree] = std::move(result.dom_tree);
        [tab elementRegions] = std::move(result.element_regions);
        if (auto& jsEngine = [tab jsEngine]; jsEngine && jsEngine->context()) {
            const std::string scale = std::to_string(static_cast<double>(scaleFactor));
            const std::string logicalWidth = std::to_string(width);
            const std::string logicalHeight = std::to_string(height);
            const std::string syncWindowMetrics =
                "globalThis.devicePixelRatio=" + scale + ";"
                "globalThis.__vibrowserScaleFactor=" + scale + ";"
                "globalThis.innerWidth=" + logicalWidth + ";"
                "globalThis.innerHeight=" + logicalHeight + ";"
                "globalThis.outerWidth=" + logicalWidth + ";"
                "globalThis.outerHeight=" + logicalHeight + ";";
            jsEngine->evaluate(syncWindowMetrics, "<vibrowser-window-metrics>");
        }

        // Extract overscroll-behavior and scroll-behavior from the html/body element
        // in the layout tree. Per CSS spec, these properties on the root scroller
        // (html or body) control the viewport's scroll boundary and animation behavior.
        tab.renderView.overscrollBehaviorX = 0;
        tab.renderView.overscrollBehaviorY = 0;
        tab.renderView.scrollBehaviorSmooth = NO;
        if (result.root) {
            // Walk the layout tree to find html or body with scroll properties set
            std::function<void(const clever::layout::LayoutNode&)> findScrollProps;
            findScrollProps = [&](const clever::layout::LayoutNode& n) {
                if (n.tag_name == "html" || n.tag_name == "body") {
                    if (n.overscroll_behavior_x != 0)
                        tab.renderView.overscrollBehaviorX = n.overscroll_behavior_x;
                    if (n.overscroll_behavior_y != 0)
                        tab.renderView.overscrollBehaviorY = n.overscroll_behavior_y;
                    if (n.scroll_behavior == 1)
                        tab.renderView.scrollBehaviorSmooth = YES;
                }
                for (auto& c : n.children) findScrollProps(*c);
            };
            findScrollProps(*result.root);
        }

        // Collect position:sticky elements from the layout tree.
        // For each sticky element, extract its absolute position, CSS top offset,
        // container bounds, and a copy of its rendered pixels from the renderer buffer
        // so that RenderView can composite them at their "stuck" position during scroll.
        std::vector<StickyElementInfo> stickyElements;
        if (result.root && result.renderer) {
            const auto& pixels = result.renderer->pixels();
            int rw = result.renderer->width();
            int rh = result.renderer->height();

            // Walk the layout tree to compute absolute positions and find sticky nodes.
            // We pass parent_abs_x/y to accumulate absolute coordinates, plus the
            // parent container's top/bottom bounds for constraining the stick range.
            std::function<void(const clever::layout::LayoutNode&, float, float, float, float)> collectSticky;
            collectSticky = [&](const clever::layout::LayoutNode& n, float parent_abs_x, float parent_abs_y,
                                float container_top, float container_bottom) {
                const auto& g = n.geometry;
                float abs_x = parent_abs_x + g.x;
                float abs_y = parent_abs_y + g.y;
                float border_box_h = g.border_box_height();
                float border_box_w = g.border_box_width();

                // Update container bounds for children: if this node is a block-level
                // container, its content area defines the scrollable constraint region.
                float child_container_top = container_top;
                float child_container_bottom = container_bottom;
                if (!n.is_text && n.position_type != 4) {
                    // Use this node's content area as the container for sticky children
                    float node_top = abs_y;
                    float node_bottom = abs_y + g.margin_box_height();
                    // Only update if this container is smaller (more restrictive)
                    if (node_bottom > node_top + 1) {
                        child_container_top = node_top;
                        child_container_bottom = node_bottom;
                    }
                }

                if (n.position_type == 4 && n.pos_top_set) {
                    // This is a position:sticky element with a top offset
                    StickyElementInfo info;
                    info.abs_y = abs_y;
                    info.height = border_box_h;
                    info.top_offset = n.pos_top;
                    info.container_top = container_top;
                    info.container_bottom = container_bottom;

                    // Compute pixel rect in the rendered buffer
                    int px_x = std::max(0, static_cast<int>(abs_x));
                    int px_y = std::max(0, static_cast<int>(abs_y));
                    int px_w = std::min(static_cast<int>(border_box_w), rw - px_x);
                    int px_h = std::min(static_cast<int>(border_box_h), rh - px_y);

                    if (px_w > 0 && px_h > 0) {
                        info.pixel_x = px_x;
                        info.pixel_width = px_w;
                        info.pixel_height = px_h;
                        // Copy the pixel region from the rendered buffer (RGBA, row-major)
                        info.pixels.resize(static_cast<size_t>(px_w) * px_h * 4);
                        for (int row = 0; row < px_h; row++) {
                            int src_offset = ((px_y + row) * rw + px_x) * 4;
                            int dst_offset = row * px_w * 4;
                            if (src_offset + px_w * 4 <= static_cast<int>(pixels.size())) {
                                std::memcpy(info.pixels.data() + dst_offset,
                                            pixels.data() + src_offset,
                                            static_cast<size_t>(px_w) * 4);
                            }
                        }
                        stickyElements.push_back(std::move(info));
                    }
                }

                // Recurse into children with accumulated offsets
                float child_x = abs_x + g.border.left + g.padding.left;
                float child_y = abs_y + g.border.top + g.padding.top;
                for (auto& c : n.children) {
                    collectSticky(*c, child_x, child_y, child_container_top, child_container_bottom);
                }
            };

            float page_bottom = static_cast<float>(rh);
            collectSticky(*result.root, 0, 0, 0, page_bottom);
        }
        [tab.renderView updateStickyElements:std::move(stickyElements)];

        // Store layout root for CSS transition property access
        [tab layoutRoot] = std::move(result.root);

        if (!result.page_title.empty()) {
            NSString* renderedTitle = [NSString stringWithUTF8String:result.page_title.c_str()];
            tab.title = normalized_tab_title(renderedTitle);
            if (tab.title.length == 0) {
                tab.title = tab.currentURL.length > 0 ? tab.currentURL : @"New Tab";
            }
            self.window.title = [NSString stringWithFormat:@"%@ - %@", tab.title, kBrowserAppName];
        } else {
            tab.title = tab.currentURL.length > 0 ? tab.currentURL : @"New Tab";
            self.window.title = kBrowserAppName;
        }
        [self rebuildTabBar];

        // Fetch favicon in background
        if (!result.favicon_url.empty()) {
            std::string faviconUrl = result.favicon_url;
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{
                clever::net::HttpClient client;
                client.set_timeout(std::chrono::seconds(5));
                clever::net::Request req;
                req.url = faviconUrl;
                req.method = clever::net::Method::GET;
                req.parse_url();
                apply_browser_request_headers(req, "image/*");
                auto resp = client.fetch(req);
                if (!resp || resp->status >= 400 || resp->body.empty()) return;

                // Decode favicon image using NSImage
                NSData* data = [NSData dataWithBytes:resp->body.data() length:resp->body.size()];
                NSImage* img = [[NSImage alloc] initWithData:data];
                if (!img) return;

                // Resize to 16x16 for tab display
                NSImage* resized = [[NSImage alloc] initWithSize:NSMakeSize(16, 16)];
                [resized lockFocus];
                [img drawInRect:NSMakeRect(0, 0, 16, 16)
                       fromRect:NSZeroRect
                      operation:NSCompositingOperationSourceOver
                       fraction:1.0];
                [resized unlockFocus];

                dispatch_async(dispatch_get_main_queue(), ^{
                    BrowserTab* currentTab = [self activeTab];
                    if (currentTab == tab) {
                        tab.faviconImage = resized;
                        [self rebuildTabBar];
                    }
                });
            });
        }

        // Handle <meta http-equiv="refresh"> auto-redirect/reload
        if (result.meta_refresh_delay >= 0) {
            [_metaRefreshTimer invalidate];
            _metaRefreshTimer = nil;

            NSString* targetURL = nil;
            if (!result.meta_refresh_url.empty()) {
                // Resolve relative URL against base URL
                std::string url = result.meta_refresh_url;
                if (url.find("://") == std::string::npos && !url.empty()) {
                    std::string base = [tab currentBaseURL];
                    if (url[0] == '/') {
                        auto scheme_end = base.find("://");
                        if (scheme_end != std::string::npos) {
                            auto host_end = base.find('/', scheme_end + 3);
                            if (host_end == std::string::npos)
                                url = base + url;
                            else
                                url = base.substr(0, host_end) + url;
                        }
                    } else {
                        auto last_slash = base.rfind('/');
                        if (last_slash != std::string::npos)
                            url = base.substr(0, last_slash + 1) + url;
                    }
                }
                targetURL = [NSString stringWithUTF8String:url.c_str()];
            }

            if (result.meta_refresh_delay == 0) {
                // Immediate redirect
                if (targetURL) {
                    // Use dispatch_async to avoid re-entering doRender synchronously
                    dispatch_async(dispatch_get_main_queue(), ^{
                        [self navigateToURL:targetURL];
                    });
                } else {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        [self reload:nil];
                    });
                }
            } else {
                // Delayed redirect/reload
                NSTimeInterval delay = static_cast<NSTimeInterval>(result.meta_refresh_delay);
                NSDictionary* userInfo = targetURL ? @{@"url": targetURL} : @{};
                _metaRefreshTimer = [NSTimer scheduledTimerWithTimeInterval:delay
                                                                    target:self
                                                                  selector:@selector(metaRefreshFired:)
                                                                  userInfo:userInfo
                                                                   repeats:NO];
            }
        }
    } else {
        const std::string detail =
            result.error.empty() ? "Unknown render failure." : result.error;
        const std::string error_html =
            build_shell_message_html("Render Error", "Render Error", detail);
        auto fallback = clever::paint::render_html(error_html, renderWidth, renderHeight);
        if (fallback.success && fallback.renderer) {
            [tab.renderView updateWithRenderer:fallback.renderer.get()];
        }
        self.window.title = [NSString stringWithFormat:@"Error - %@", kBrowserAppName];
    }
}

- (void)goBack:(id)sender {
    (void)sender;
    BrowserTab* tab = [self activeTab];
    if (!tab || tab.historyIndex <= 0) return;

    tab.historyIndex--;
    NSString* url = tab.history[tab.historyIndex];
    [_addressBar setStringValue:url];
    tab.currentURL = url;
    [self updateNavButtons];

    std::string urlStr([url UTF8String]);
    [_spinner startAnimation:nil];
    [self startProgressForNavigation];
    self.window.title = [NSString stringWithFormat:@"Loading... - %@", kBrowserAppName];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [self fetchAndRender:urlStr];
    });
}

- (void)goForward:(id)sender {
    (void)sender;
    BrowserTab* tab = [self activeTab];
    if (!tab || tab.historyIndex >= (NSInteger)tab.history.count - 1) return;

    tab.historyIndex++;
    NSString* url = tab.history[tab.historyIndex];
    [_addressBar setStringValue:url];
    tab.currentURL = url;
    [self updateNavButtons];

    std::string urlStr([url UTF8String]);
    [_spinner startAnimation:nil];
    [self startProgressForNavigation];
    self.window.title = [NSString stringWithFormat:@"Loading... - %@", kBrowserAppName];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [self fetchAndRender:urlStr];
    });
}

- (void)goHome:(id)sender {
    (void)sender;
    BrowserTab* tab = [self activeTab];
    if (!tab) return;
    // Clear URL and re-render the welcome page
    tab.currentURL = @"";
    [_addressBar setStringValue:@""];
    [tab currentHTML] = "";
    [tab currentBaseURL] = "";
    // The welcome page is rendered by main.mm's getWelcomeHTML()
    // Re-create a new tab effect by clearing and rendering welcome
    extern std::string getWelcomeHTML();
    std::string html = getWelcomeHTML();
    [tab currentHTML] = html;
    [self doRender:html];
}

- (void)reload:(id)sender {
    (void)sender;
    BrowserTab* tab = [self activeTab];
    if (!tab) return;

    NSString* url = tab.currentURL;
    BOOL hasWebURL =
        (url.length > 0) &&
        ([url hasPrefix:@"http://"] || [url hasPrefix:@"https://"]);

    if (hasWebURL) {
        std::string urlStr([url UTF8String]);
        [_spinner startAnimation:nil];
        [self startProgressForNavigation];
        self.window.title = [NSString stringWithFormat:@"Loading... - %@", kBrowserAppName];
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            [self fetchAndRender:urlStr];
        });
        return;
    }

    if (![tab currentHTML].empty()) {
        [self doRender:[tab currentHTML]];
    } else if (tab.historyIndex >= 0) {
        NSString* historyURL = tab.history[tab.historyIndex];
        std::string urlStr([historyURL UTF8String]);
        [_spinner startAnimation:nil];
        [self startProgressForNavigation];
        self.window.title = [NSString stringWithFormat:@"Loading... - %@", kBrowserAppName];
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            [self fetchAndRender:urlStr];
        });
    }
}

- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    [self layoutChromeViews];
    [_resizeTimer invalidate];
    _resizeTimer = nil;
    BrowserTab* tab = [self activeTab];
    if (tab && ![tab currentHTML].empty()) {
        [self doRender:[tab currentHTML]];
    }
}

- (void)resizeTimerFired:(NSTimer*)timer {
    (void)timer;
    _resizeTimer = nil;
    BrowserTab* tab = [self activeTab];
    if (tab && ![tab currentHTML].empty()) {
        [self doRender:[tab currentHTML]];
    }
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
    (void)notification;
    [self layoutChromeViews];
    BrowserTab* tab = [self activeTab];
    if (tab && ![tab currentHTML].empty()) {
        [self doRender:[tab currentHTML]];
    }
}

- (void)metaRefreshFired:(NSTimer*)timer {
    _metaRefreshTimer = nil;
    NSDictionary* info = timer.userInfo;
    NSString* url = info[@"url"];
    if (url && url.length > 0) {
        [self navigateToURL:url];
    } else {
        [self reload:nil];
    }
}

- (void)updateNavButtons {
    BrowserTab* tab = [self activeTab];
    _backButton.enabled = (tab && tab.historyIndex > 0);
    _forwardButton.enabled = (tab && tab.historyIndex < (NSInteger)tab.history.count - 1);
}

#pragma mark - View Source

- (void)viewSource {
    BrowserTab* tab = [self activeTab];
    if (!tab) return;

    std::string html = [tab currentHTML];
    if (html.empty()) return;

    // Syntax-highlighted HTML source view
    // Colors: tags=#569cd6, attributes=#9cdcfe, strings=#ce9178, comments=#6a9955, doctype=#c586c0
    std::string highlighted;
    highlighted.reserve(html.size() * 2);
    size_t i = 0;
    size_t len = html.size();

    while (i < len) {
        // HTML comment: <!-- ... -->
        if (i + 4 <= len && html.substr(i, 4) == "<!--") {
            highlighted += "<span style=\"color:#6a9955\">&lt;!--";
            i += 4;
            while (i < len) {
                if (i + 3 <= len && html.substr(i, 3) == "-->") {
                    highlighted += "--&gt;</span>";
                    i += 3;
                    break;
                }
                if (html[i] == '<') highlighted += "&lt;";
                else if (html[i] == '>') highlighted += "&gt;";
                else if (html[i] == '&') highlighted += "&amp;";
                else highlighted += html[i];
                i++;
            }
            continue;
        }
        // DOCTYPE
        if (i + 2 <= len && html[i] == '<' && html[i+1] == '!') {
            highlighted += "<span style=\"color:#c586c0\">&lt;!";
            i += 2;
            while (i < len && html[i] != '>') {
                if (html[i] == '<') highlighted += "&lt;";
                else if (html[i] == '&') highlighted += "&amp;";
                else if (html[i] == '"') highlighted += "&quot;";
                else highlighted += html[i];
                i++;
            }
            if (i < len) { highlighted += "&gt;</span>"; i++; }
            continue;
        }
        // HTML tag
        if (html[i] == '<') {
            highlighted += "<span style=\"color:#569cd6\">&lt;";
            i++;
            // Closing slash
            if (i < len && html[i] == '/') { highlighted += '/'; i++; }
            // Tag name
            while (i < len && html[i] != '>' && html[i] != ' ' && html[i] != '\n' && html[i] != '\t') {
                if (html[i] == '&') highlighted += "&amp;";
                else highlighted += html[i];
                i++;
            }
            highlighted += "</span>";
            // Attributes
            while (i < len && html[i] != '>') {
                if (html[i] == '"' || html[i] == '\'') {
                    char quote = html[i];
                    highlighted += "<span style=\"color:#ce9178\">";
                    highlighted += (quote == '"' ? "&quot;" : "'");
                    i++;
                    while (i < len && html[i] != quote) {
                        if (html[i] == '<') highlighted += "&lt;";
                        else if (html[i] == '>') highlighted += "&gt;";
                        else if (html[i] == '&') highlighted += "&amp;";
                        else if (html[i] == '"') highlighted += "&quot;";
                        else highlighted += html[i];
                        i++;
                    }
                    if (i < len) { highlighted += (quote == '"' ? "&quot;" : "'"); i++; }
                    highlighted += "</span>";
                } else if (html[i] == '=' || html[i] == ' ' || html[i] == '\n' || html[i] == '\t' || html[i] == '/') {
                    highlighted += html[i];
                    i++;
                } else {
                    // Attribute name
                    highlighted += "<span style=\"color:#9cdcfe\">";
                    while (i < len && html[i] != '=' && html[i] != '>' && html[i] != ' ' && html[i] != '\n' && html[i] != '/') {
                        highlighted += html[i];
                        i++;
                    }
                    highlighted += "</span>";
                }
            }
            if (i < len) {
                highlighted += "<span style=\"color:#569cd6\">&gt;</span>";
                i++;
            }
            continue;
        }
        // Entity references
        if (html[i] == '&') { highlighted += "&amp;"; i++; continue; }
        // Normal text
        highlighted += html[i];
        i++;
    }

    const std::string safe_title = escape_html_text(std::string([tab.title UTF8String]));
    std::string source_html =
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>Source: " + safe_title + "</title>"
        "<style>:root{color-scheme:dark;}*{box-sizing:border-box;}html,body{margin:0;padding:0;}"
        "body{background-color:#111827;color:#e5e7eb;-webkit-text-size-adjust:100%;}"
        "pre{margin:0;min-height:100vh;padding:14px 16px;font-size:13px;line-height:1.5;"
        "font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;"
        "white-space:pre-wrap;overflow-wrap:anywhere;tab-size:2;}"
        "@media (max-width:640px){pre{padding:12px;font-size:12px;}}</style></head>"
        "<body><pre>" + highlighted + "</pre></body></html>";

    [self newTab];
    BrowserTab* newTab = [self activeTab];
    newTab.title = [NSString stringWithFormat:@"Source: %@", tab.title];
    [newTab currentHTML] = source_html;
    [self doRender:source_html];
    [self rebuildTabBar];
}

#pragma mark - RenderViewDelegate

- (void)renderView:(RenderView*)view didClickLink:(NSString*)href {
    (void)view;
    if (href && href.length > 0) {
        // Handle fragment/anchor links (#section) by scrolling instead of navigating
        if ([href hasPrefix:@"#"]) {
            NSString* fragment = [href substringFromIndex:1];
            if (fragment.length > 0) {
                BrowserTab* tab = [self activeTab];
                if (tab) {
                    std::string frag([fragment UTF8String]);
                    auto& positions = [tab idPositions];
                    auto it = positions.find(frag);
                    if (it != positions.end()) {
                        tab.renderView.scrollOffset = [tab.renderView viewOffsetForRendererY:it->second];
                        [tab.renderView setNeedsDisplay:YES];
                    }
                }
            }
            return;
        }
        // Handle same-page full URL with fragment (e.g., https://same.page/path#section)
        NSRange hashRange = [href rangeOfString:@"#"];
        if (hashRange.location != NSNotFound) {
            BrowserTab* tab = [self activeTab];
            if (tab && tab.currentURL.length > 0) {
                NSString* hrefBase = [href substringToIndex:hashRange.location];
                // Strip fragment from current URL for comparison
                NSString* currentBase = tab.currentURL;
                NSRange currentHash = [currentBase rangeOfString:@"#"];
                if (currentHash.location != NSNotFound) {
                    currentBase = [currentBase substringToIndex:currentHash.location];
                }
                if ([hrefBase isEqualToString:currentBase] || hrefBase.length == 0) {
                    NSString* fragment = [href substringFromIndex:hashRange.location + 1];
                    if (fragment.length > 0) {
                        std::string frag([fragment UTF8String]);
                        auto& positions = [tab idPositions];
                        auto it = positions.find(frag);
                        if (it != positions.end()) {
                            tab.renderView.scrollOffset = [tab.renderView viewOffsetForRendererY:it->second];
                            [tab.renderView setNeedsDisplay:YES];
                            // Update URL bar to show fragment
                            tab.currentURL = href;
                            [_addressBar setStringValue:href];
                        }
                    }
                    return;
                }
            }
        }
        [self navigateToURL:href];
    }
}

- (void)renderView:(RenderView*)view didClickLinkInNewTab:(NSString*)href {
    (void)view;
    if (href && href.length > 0) {
        [self newTab];
        [self navigateToURL:href];
    }
}

- (void)renderView:(RenderView*)view didSubmitForm:(const clever::paint::FormData&)formData {
    (void)view;
    [self submitForm:formData];
}

- (void)submitForm:(const clever::paint::FormData&)formData {
    BrowserTab* tab = [self activeTab];
    if (!tab) return;

    std::string action = formData.action;
    if (action.empty()) return;

    std::string method = formData.method;

    // URL-encode form fields into a query string
    auto url_encode = [](const std::string& str) -> std::string {
        std::string encoded;
        for (unsigned char c : str) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += c;
            } else if (c == ' ') {
                encoded += '+';
            } else {
                char hex[4];
                snprintf(hex, sizeof(hex), "%%%02X", c);
                encoded += hex;
            }
        }
        return encoded;
    };

    // Build form-encoded body from fields
    std::string body;
    for (auto& field : formData.fields) {
        // Skip fields that should not be submitted
        if (field.name.empty()) continue;
        if (field.type == "submit" || field.type == "button" || field.type == "reset") continue;
        if ((field.type == "checkbox" || field.type == "radio") && !field.checked) continue;

        if (!body.empty()) body += "&";
        body += url_encode(field.name) + "=" + url_encode(field.value);
    }

    if (method == "get") {
        // GET: append to URL and navigate
        std::string url = action;
        if (!body.empty()) {
            if (url.find('?') != std::string::npos)
                url += "&" + body;
            else
                url += "?" + body;
        }
        [self navigateToURL:[NSString stringWithUTF8String:url.c_str()]];
        return;
    }

    // POST: send request with body
    NSString* urlStr = [NSString stringWithUTF8String:action.c_str()];
    [_addressBar setStringValue:urlStr];
    tab.currentURL = urlStr;

    // Add to history
    while ((NSInteger)tab.history.count > tab.historyIndex + 1) {
        [tab.history removeLastObject];
    }
    [tab.history addObject:urlStr];
    tab.historyIndex = tab.history.count - 1;
    [self updateNavButtons];

    [_spinner startAnimation:nil];
    [self startProgressForNavigation];
    self.window.title = [NSString stringWithFormat:@"Loading... - %@", kBrowserAppName];

    std::string actionUrl = action;
    std::string postBody = body;
    std::string enctype = formData.enctype;

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [self fetchAndRenderPOST:actionUrl body:postBody contentType:enctype];
    });
}

- (void)fetchAndRenderPOST:(const std::string&)urlStr
                       body:(const std::string&)postBody
                contentType:(const std::string&)enctype {
    clever::net::HttpClient client;
    client.set_timeout(std::chrono::seconds(10));
    client.set_max_redirects(0);

    std::string currentUrl = normalize_url_for_compat(urlStr);
    std::optional<clever::net::Response> response;
    std::string lastFetchedUrl = currentUrl;
    int redirects = 0;
    const int maxRedirects = 10;

    auto& jar = clever::net::CookieJar::shared();

    // First request is POST; subsequent redirects become GET (per HTTP 303 behavior,
    // and common browser behavior for 301/302 as well)
    bool is_first_request = true;

    while (redirects < maxRedirects) {
        currentUrl = normalize_url_for_compat(currentUrl);
        clever::net::Request req;
        req.url = currentUrl;
        req.parse_url();
        apply_browser_request_headers(req, "text/html,application/xhtml+xml,*/*;q=0.8");

        if (is_first_request) {
            req.method = clever::net::Method::POST;
            req.headers.set("Content-Type", enctype);
            req.body.assign(postBody.begin(), postBody.end());
            std::string content_length = std::to_string(postBody.size());
            req.headers.set("Content-Length", content_length);
            is_first_request = false;
        } else {
            req.method = clever::net::Method::GET;
        }

        // Attach cookies
        std::string cookies = jar.get_cookie_header(req.host, req.path, req.use_tls);
        if (!cookies.empty()) {
            req.headers.set("Cookie", cookies);
        }

        response = client.fetch(req);

        if (!response) break;
        const std::string responseUrl = response->url.empty() ? currentUrl : response->url;
        lastFetchedUrl = responseUrl;

        // Store cookies from response
        for (auto& cookie_val : response->headers.get_all("set-cookie")) {
            jar.set_from_header(cookie_val, req.host);
        }

        // Follow redirects
        if (response->status == 301 || response->status == 302 ||
            response->status == 303 || response->status == 307 ||
            response->status == 308) {
            auto location = response->headers.get("location");
            if (!location || location->empty()) break;
            currentUrl = resolve_url_reference(*location, responseUrl);
            redirects++;
            // For 307/308, preserve POST method; for 301/302/303, switch to GET
            if (response->status == 307 || response->status == 308) {
                is_first_request = true; // keep POST
            }
            continue;
        }
        break;
    }

    std::string finalUrl = lastFetchedUrl;

    dispatch_async(dispatch_get_main_queue(), ^{
        [_spinner stopAnimation:nil];

        BrowserTab* tab = [self activeTab];
        if (!tab) return;

        // Response received — jump progress to 70%
        [self setProgress:0.7 animated:YES];

        if (!response || response->status >= 400) {
            std::string error_html;
            if (!response) {
                error_html = build_shell_message_html(
                    "Connection Failed",
                    "Connection Failed",
                    "Could not connect to " + urlStr);
            } else {
                const std::string status_heading = "Error " + std::to_string(response->status);
                error_html = build_shell_message_html(
                    status_heading,
                    status_heading,
                    response->status_text);
            }
            [self renderHTML:[NSString stringWithUTF8String:error_html.c_str()]];
            [self finishProgress];
            return;
        }

        // Update address bar with final URL after redirects
        if (finalUrl != urlStr) {
            tab.currentURL = [NSString stringWithUTF8String:finalUrl.c_str()];
            [_addressBar setStringValue:tab.currentURL];
        }

        std::string html = response->body_as_string();
        [tab currentHTML] = html;
        [tab currentBaseURL] = finalUrl;

        // Rendering starts — set progress to 90%
        [self setProgress:0.9 animated:YES];

        [self doRender:html];

        // Rendering complete — finish progress bar
        [self finishProgress];
    });
}

- (void)renderView:(RenderView*)view hoveredLink:(NSString*)url {
    (void)view;
    [_statusBarHideTimer invalidate];
    _statusBarHideTimer = nil;

    if (url && url.length > 0) {
        // Show the status bar with the hovered URL
        // Add a small left padding by prepending a space
        [_statusBar setStringValue:[NSString stringWithFormat:@" %@", url]];

        // Resize width to fit text, capped at 60% of content area width
        NSSize textSize = [[_statusBar attributedStringValue]
            boundingRectWithSize:NSMakeSize(CGFLOAT_MAX, 20)
                         options:NSStringDrawingUsesLineFragmentOrigin].size;
        CGFloat maxWidth = _contentArea.bounds.size.width * 0.6;
        CGFloat width = std::min(textSize.width + 12.0, maxWidth);
        _statusBar.frame = NSMakeRect(0, 0, width, 20);

        if (_statusBar.hidden || _statusBar.alphaValue < 1.0) {
            _statusBar.hidden = NO;
            [NSAnimationContext runAnimationGroup:^(NSAnimationContext* ctx) {
                ctx.duration = 0.15;
                _statusBar.animator.alphaValue = 1.0;
            }];
        }
    } else {
        // Hide the status bar after a short delay (avoids flicker when moving between links)
        _statusBarHideTimer = [NSTimer scheduledTimerWithTimeInterval:0.1
            target:self
            selector:@selector(hideStatusBar:)
            userInfo:nil
            repeats:NO];
    }
}

- (void)hideStatusBar:(NSTimer*)timer {
    (void)timer;
    _statusBarHideTimer = nil;
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext* ctx) {
        ctx.duration = 0.15;
        _statusBar.animator.alphaValue = 0.0;
    } completionHandler:^{
        if (_statusBar.alphaValue < 0.01) {
            _statusBar.hidden = YES;
        }
    }];
}

- (void)renderViewGoBack:(RenderView*)view {
    (void)view;
    [self goBack:nil];
}

- (void)renderViewGoForward:(RenderView*)view {
    (void)view;
    [self goForward:nil];
}

- (void)renderViewReload:(RenderView*)view {
    (void)view;
    [self reload:nil];
}

- (void)renderViewViewSource:(RenderView*)view {
    (void)view;
    [self viewSource];
}

- (void)renderViewSaveScreenshot:(RenderView*)view {
    (void)view;
    [self saveScreenshot];
}

- (void)renderView:(RenderView*)view didToggleDetails:(int)detailsId {
    (void)view;
    // Toggle the details element: add if not present, remove if already toggled
    if (_toggledDetails.count(detailsId)) {
        _toggledDetails.erase(detailsId);
    } else {
        _toggledDetails.insert(detailsId);
    }
    // Re-render the page with updated toggle state (preserves scroll position)
    BrowserTab* tab = [self activeTab];
    if (tab) {
        [self doRender:[tab currentHTML]];
    }
}

- (void)renderView:(RenderView*)view didSelectOption:(NSString*)optionText
           atIndex:(int)index forSelectNamed:(NSString*)name {
    (void)view;
    BrowserTab* tab = [self activeTab];
    if (!tab) return;

    auto& domTree = [tab domTree];
    if (!domTree) return;

    std::string selectName([name UTF8String]);

    // Walk the DOM tree to find the <select> element with the matching name attribute.
    clever::html::SimpleNode* selectNode = nullptr;
    std::function<void(clever::html::SimpleNode*)> findSelect;
    findSelect = [&](clever::html::SimpleNode* node) {
        if (selectNode) return; // already found
        if (node->type == clever::html::SimpleNode::Element &&
            node->tag_name == "select" && get_attr(node, "name") == selectName) {
            selectNode = node;
            return;
        }
        for (auto& child : node->children) {
            findSelect(child.get());
        }
    };
    findSelect(domTree.get());

    if (!selectNode) return;

    // Update option selected states: remove "selected" from all options,
    // then add "selected" to the option at the chosen index.
    int optionIdx = 0;
    for (auto& child : selectNode->children) {
        if (child->type == clever::html::SimpleNode::Element && child->tag_name == "option") {
            if (optionIdx == index) {
                set_attr(child.get(), "selected", "");
            } else {
                remove_attr(child.get(), "selected");
            }
            optionIdx++;
        }
    }

    // Serialize the updated DOM tree back to HTML so re-render reflects the change.
    [tab currentHTML] = serialize_dom_node(domTree.get());

    // Dispatch a "change" event to JS handlers on the <select> element.
    auto& jsEngine = [tab jsEngine];
    if (jsEngine) {
        clever::js::dispatch_event(jsEngine->context(), selectNode, "change");
    }

    // Re-render the page with updated state (preserves scroll position).
    [self doRender:[tab currentHTML]];
}

- (BOOL)renderView:(RenderView*)view didClickElementAtX:(float)x y:(float)y {
    (void)view;
    BrowserTab* tab = [self activeTab];
    if (!tab) return NO;

    auto& jsEngine = [tab jsEngine];
    auto& domTree = [tab domTree];
    auto& elementRegions = [tab elementRegions];
    if (!jsEngine || elementRegions.empty()) return NO;

    // Hit-test against element regions in reverse order (last = topmost / highest z-order).
    // Find the smallest (most specific) element that contains the click point.
    clever::html::SimpleNode* target = nullptr;
    float smallest_area = std::numeric_limits<float>::max();
    for (auto it = elementRegions.rbegin(); it != elementRegions.rend(); ++it) {
        if (it->bounds.contains(x, y) && it->dom_node) {
            float area = it->bounds.width * it->bounds.height;
            if (area < smallest_area) {
                smallest_area = area;
                target = static_cast<clever::html::SimpleNode*>(it->dom_node);
            }
        }
    }

    if (!target) return NO;

    // Dispatch mousedown → mouseup → click event sequence with coordinates
    clever::js::dispatch_mouse_event(jsEngine->context(), target, "mousedown",
        x, y, x, y, 0, 1, false, false, false, false, 1);
    clever::js::dispatch_mouse_event(jsEngine->context(), target, "mouseup",
        x, y, x, y, 0, 0, false, false, false, false, 1);

    // Dispatch the "click" event to the target DOM element.
    // Returns true if event.preventDefault() was called.
    bool prevented = clever::js::dispatch_mouse_event(
        jsEngine->context(), target, "click",
        x, y, x, y, 0, 0, false, false, false, false, 1);

    // ---- Interactive form control handling (checkbox / radio) ----
    // After JS click dispatch, if not prevented, toggle checkbox/radio state.
    // Find the <input> element: either the target itself, or the first <input>
    // child if the target is a <label>, or walk up to find a containing <label>.
    bool formControlToggled = false;
    if (!prevented && domTree) {
        clever::html::SimpleNode* inputNode = nullptr;

        // Check if the target IS a checkbox/radio input
        if (target->type == clever::html::SimpleNode::Element &&
            target->tag_name == "input") {
            std::string inputType = get_attr(target, "type");
            if (inputType == "checkbox" || inputType == "radio") {
                inputNode = target;
            }
        }

        // If not, check if we clicked inside a <label> that contains an input
        if (!inputNode) {
            // Walk up from the target to find a <label> ancestor
            clever::html::SimpleNode* labelNode = nullptr;
            clever::html::SimpleNode* walk = target;
            while (walk && !labelNode) {
                if (walk->type == clever::html::SimpleNode::Element && walk->tag_name == "label") {
                    labelNode = walk;
                }
                walk = walk->parent;
            }
            if (labelNode) {
                // Check if <label> has a "for" attribute referencing an input by id
                std::string forId = get_attr(labelNode, "for");
                if (!forId.empty()) {
                    // Find the input with that id in the DOM tree
                    std::function<clever::html::SimpleNode*(clever::html::SimpleNode*)> findById;
                    findById = [&](clever::html::SimpleNode* node) -> clever::html::SimpleNode* {
                        if (node->type == clever::html::SimpleNode::Element &&
                            get_attr(node, "id") == forId) {
                            return node;
                        }
                        for (auto& child : node->children) {
                            auto* found = findById(child.get());
                            if (found) return found;
                        }
                        return nullptr;
                    };
                    auto* found = findById(domTree.get());
                    if (found && found->tag_name == "input") {
                        std::string ft = get_attr(found, "type");
                        if (ft == "checkbox" || ft == "radio") {
                            inputNode = found;
                        }
                    }
                } else {
                    // Implicit label association: find first <input> child of the label
                    std::function<clever::html::SimpleNode*(clever::html::SimpleNode*)> findInput;
                    findInput = [&](clever::html::SimpleNode* node) -> clever::html::SimpleNode* {
                        for (auto& child : node->children) {
                            if (child->type == clever::html::SimpleNode::Element &&
                                child->tag_name == "input") {
                                std::string ct = get_attr(child.get(), "type");
                                if (ct == "checkbox" || ct == "radio") {
                                    return child.get();
                                }
                            }
                            auto* found = findInput(child.get());
                            if (found) return found;
                        }
                        return nullptr;
                    };
                    inputNode = findInput(labelNode);
                }
            }
        }

        // Toggle the checkbox/radio state if we found an input
        if (inputNode) {
            std::string inputType = get_attr(inputNode, "type");

            if (inputType == "checkbox") {
                // Toggle checked attribute
                if (has_attr(inputNode, "checked")) {
                    remove_attr(inputNode, "checked");
                } else {
                    set_attr(inputNode, "checked", "");
                }
                formControlToggled = true;
            } else if (inputType == "radio") {
                // Uncheck all radio buttons with the same name in the same form,
                // then check this one.
                std::string radioName = get_attr(inputNode, "name");
                if (!radioName.empty()) {
                    // Find all <input type="radio"> with the same name in the tree
                    std::function<void(clever::html::SimpleNode*)> uncheckRadios;
                    uncheckRadios = [&](clever::html::SimpleNode* node) {
                        if (node->type == clever::html::SimpleNode::Element &&
                            node->tag_name == "input" &&
                            get_attr(node, "type") == "radio" &&
                            get_attr(node, "name") == radioName) {
                            remove_attr(node, "checked");
                        }
                        for (auto& child : node->children) {
                            uncheckRadios(child.get());
                        }
                    };
                    uncheckRadios(domTree.get());
                }
                set_attr(inputNode, "checked", "");
                formControlToggled = true;
            }

            if (formControlToggled) {
                // Dispatch "change" event on the input element
                clever::js::dispatch_event(jsEngine->context(), inputNode, "change");

                // Serialize DOM back to HTML to persist the state across re-render
                [tab currentHTML] = serialize_dom_node(domTree.get());
            }
        }
    }

    // ---- Range slider click-to-set ----
    if (!prevented && !formControlToggled && domTree && target) {
        if (target->type == clever::html::SimpleNode::Element &&
            target->tag_name == "input" && get_attr(target, "type") == "range") {
            // Get range attributes
            float rangeMin = 0, rangeMax = 100;
            std::string smin = get_attr(target, "min");
            std::string smax = get_attr(target, "max");
            std::string sstep = get_attr(target, "step");
            if (!smin.empty()) rangeMin = std::stof(smin);
            if (!smax.empty()) rangeMax = std::stof(smax);
            float step = sstep.empty() ? 1.0f : std::stof(sstep);

            // Find element bounds
            clever::paint::Rect bounds = {0, 0, 0, 0};
            for (auto& region : elementRegions) {
                if (region.dom_node == target) {
                    bounds = region.bounds;
                    break;
                }
            }
            if (bounds.width > 0) {
                // Calculate value from click position within bounds
                float ratio = (x - bounds.x) / bounds.width;
                ratio = std::max(0.0f, std::min(1.0f, ratio));
                float rawValue = rangeMin + ratio * (rangeMax - rangeMin);
                // Snap to step
                if (step > 0) {
                    rawValue = std::round((rawValue - rangeMin) / step) * step + rangeMin;
                }
                rawValue = std::max(rangeMin, std::min(rangeMax, rawValue));
                // Update DOM value
                std::string valStr = std::to_string(static_cast<int>(rawValue));
                set_attr(target, "value", valStr);
                // Dispatch "input" and "change" events
                clever::js::dispatch_event(jsEngine->context(), target, "input");
                clever::js::dispatch_event(jsEngine->context(), target, "change");
                [tab currentHTML] = serialize_dom_node(domTree.get());
                formControlToggled = true;
            }
        }
    }

    // ---- Color picker click ----
    if (!prevented && !formControlToggled && domTree && target) {
        if (target->type == clever::html::SimpleNode::Element &&
            target->tag_name == "input" && get_attr(target, "type") == "color") {
            // Show native macOS color picker
            std::string currentColor = get_attr(target, "value");
            if (currentColor.empty()) currentColor = "#000000";
            // Parse hex color
            uint8_t cr = 0, cg = 0, cb = 0;
            if (currentColor.size() >= 7 && currentColor[0] == '#') {
                unsigned int hex = 0;
                sscanf(currentColor.c_str() + 1, "%06x", &hex);
                cr = (hex >> 16) & 0xFF;
                cg = (hex >> 8) & 0xFF;
                cb = hex & 0xFF;
            }
            NSColor* initial = [NSColor colorWithRed:cr/255.0 green:cg/255.0
                                                blue:cb/255.0 alpha:1.0];
            NSColorPanel* panel = [NSColorPanel sharedColorPanel];
            [panel setColor:initial];
            [panel setTarget:self];
            [panel setAction:@selector(colorPanelChanged:)];
            [panel orderFront:nil];
            // Store the target node for the callback
            [tab setFocusedInputNode:target];
        }
    }

    // ---- Text input focus handling ----
    // If the clicked element is a text-like <input> or <textarea>, show an
    // overlay NSTextField for inline editing.
    if (!prevented && !formControlToggled && domTree && target) {
        bool isTextInput = false;
        bool isPassword = false;
        clever::html::SimpleNode* inputTarget = target;

        // Check if target is a text-type input
        if (inputTarget->type == clever::html::SimpleNode::Element &&
            inputTarget->tag_name == "input") {
            std::string itype = get_attr(inputTarget, "type");
            if (itype.empty()) itype = "text"; // default type
            if (itype == "text" || itype == "search" || itype == "email" ||
                itype == "url" || itype == "tel" || itype == "number" ||
                itype == "password" || itype == "date" || itype == "time" ||
                itype == "datetime-local" || itype == "week" || itype == "month") {
                isTextInput = true;
                isPassword = (itype == "password");
            }
        } else if (inputTarget->type == clever::html::SimpleNode::Element &&
                   inputTarget->tag_name == "textarea") {
            isTextInput = true;
        }

        // Also check if clicked inside a <label> that wraps a text input
        if (!isTextInput) {
            clever::html::SimpleNode* walk = target;
            while (walk && !isTextInput) {
                if (walk->type == clever::html::SimpleNode::Element && walk->tag_name == "label") {
                    // Check for= attribute
                    std::string forId = get_attr(walk, "for");
                    if (!forId.empty()) {
                        std::function<clever::html::SimpleNode*(clever::html::SimpleNode*)> findById;
                        findById = [&](clever::html::SimpleNode* node) -> clever::html::SimpleNode* {
                            if (node->type == clever::html::SimpleNode::Element &&
                                get_attr(node, "id") == forId) return node;
                            for (auto& child : node->children)
                                if (auto* f = findById(child.get())) return f;
                            return nullptr;
                        };
                        if (auto* found = findById(domTree.get())) {
                            if (found->tag_name == "input") {
                                std::string ft = get_attr(found, "type");
                                if (ft.empty()) ft = "text";
                                if (ft == "text" || ft == "search" || ft == "email" ||
                                    ft == "url" || ft == "tel" || ft == "number" ||
                                    ft == "password") {
                                    isTextInput = true;
                                    isPassword = (ft == "password");
                                    inputTarget = found;
                                }
                            } else if (found->tag_name == "textarea") {
                                isTextInput = true;
                                inputTarget = found;
                            }
                        }
                    } else {
                        // Implicit label — find first text input child
                        std::function<clever::html::SimpleNode*(clever::html::SimpleNode*)> findInput;
                        findInput = [&](clever::html::SimpleNode* node) -> clever::html::SimpleNode* {
                            for (auto& child : node->children) {
                                if (child->type == clever::html::SimpleNode::Element) {
                                    if (child->tag_name == "input") {
                                        std::string ct = get_attr(child.get(), "type");
                                        if (ct.empty()) ct = "text";
                                        if (ct == "text" || ct == "search" || ct == "email" ||
                                            ct == "url" || ct == "tel" || ct == "number" ||
                                            ct == "password") return child.get();
                                    }
                                    if (child->tag_name == "textarea") return child.get();
                                }
                                if (auto* f = findInput(child.get())) return f;
                            }
                            return nullptr;
                        };
                        if (auto* found = findInput(walk)) {
                            isTextInput = true;
                            isPassword = (found->tag_name == "input" &&
                                          get_attr(found, "type") == "password");
                            inputTarget = found;
                        }
                    }
                    break;
                }
                walk = walk->parent;
            }
        }

        if (isTextInput) {
            // Find the element's bounding rect from element regions
            clever::paint::Rect inputBounds = {0, 0, 0, 0};
            for (auto& region : elementRegions) {
                if (region.dom_node == inputTarget) {
                    inputBounds = region.bounds;
                    break;
                }
            }
            if (inputBounds.width > 0 && inputBounds.height > 0) {
                // Get current value from DOM
                std::string currentValue;
                if (inputTarget->tag_name == "input") {
                    currentValue = get_attr(inputTarget, "value");
                } else {
                    // textarea — text content
                    currentValue = inputTarget->text_content();
                }
                NSString* val = [NSString stringWithUTF8String:currentValue.c_str()];
                // Dispatch "blur" on previously focused element, "focus" on new one
                auto* prevFocused = [tab focusedInputNode];
                if (prevFocused && prevFocused != inputTarget) {
                    remove_attr(prevFocused, [kBrowserFocusAttr UTF8String]);
                    if (jsEngine)
                        clever::js::dispatch_event(jsEngine->context(), prevFocused, "blur");
                }
                [tab setFocusedInputNode:inputTarget];
                set_attr(inputTarget, [kBrowserFocusAttr UTF8String], "");
                if (jsEngine) {
                    clever::js::dispatch_event(jsEngine->context(), inputTarget, "focus");
                }
                [tab.renderView showTextInputOverlayWithBounds:inputBounds
                                                        value:val
                                                   isPassword:isPassword];
            }
        }
    }

    // Re-render if JS modified the DOM or if we toggled a form control
    if (formControlToggled || clever::js::dom_was_modified(jsEngine->context())) {
        [self doRender:[tab currentHTML]];
    }

    return prevented ? YES : NO;
}

#pragma mark - NSTextFieldDelegate

- (void)controlTextDidEndEditing:(NSNotification*)notification {
    NSInteger movement = [[[notification userInfo] objectForKey:@"NSTextMovement"] integerValue];
    if (movement == NSReturnTextMovement) {
        // Check if the find field is the sender
        NSTextField* field = [notification object];
        if (field == _findField) {
            [self performFind];
            return;
        }
        if (field == _addressBar) {
            [self addressBarSubmitted:field];
            return;
        }
    }
}

- (void)addressBarSubmitted:(id)sender {
    NSTextField* field = [sender isKindOfClass:[NSTextField class]]
        ? (NSTextField*)sender
        : _addressBar;
    NSString* text = field ? [field stringValue] : @"";
    NSString* trimmedURL = [text
        stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (trimmedURL.length == 0) {
        [self focusAddressBarAndSelectAll];
        return;
    }

    BrowserTab* tab = [self activeTab];
    if (tab) {
        [self.window makeFirstResponder:tab.renderView];
    }
    [self navigateToURL:trimmedURL];
}

- (void)goToAddressBar:(id)sender {
    (void)sender;
    [self focusAddressBarAndSelectAll];
}

- (void)openAddressBarModal {
    NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 420, 24)];
    input.stringValue = _addressBar.stringValue ? _addressBar.stringValue : @"";
    input.placeholderString = @"https://www.google.com or <h1>Hello</h1>";

    NSAlert* alert = [NSAlert new];
    alert.messageText = @"Open URL";
    alert.informativeText = @"Enter a URL or HTML content to render.";
    alert.accessoryView = input;
    [alert addButtonWithTitle:@"Go"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert.window setInitialFirstResponder:input];

    if ([alert runModal] == NSAlertFirstButtonReturn) {
        NSString* text = [input stringValue];
        text = [text
            stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (text.length > 0) {
            [self addressBarSubmitted:input];
        }
    }
}

- (void)controlTextDidBeginEditing:(NSNotification*)notification {
    NSTextField* field = [notification object];
    if (field == _addressBar) {
        // Select all text when user clicks into the address bar (browser standard behavior)
        dispatch_async(dispatch_get_main_queue(), ^{
            NSText* editor = [self.window fieldEditor:YES forObject:_addressBar];
            if (editor && [editor respondsToSelector:@selector(selectAll:)]) {
                [editor selectAll:nil];
            }
        });
    }
}

#pragma mark - RenderView Text Input Delegate

- (void)renderView:(RenderView*)view didFinishEditingInputWithValue:(NSString*)value {
    BrowserTab* tab = [self activeTab];
    if (!tab) return;
    auto* inputNode = [tab focusedInputNode];
    if (!inputNode) return;

    std::string newValue = value ? [value UTF8String] : "";

    // Update the DOM node's value
    if (inputNode->tag_name == "input") {
        set_attr(inputNode, "value", newValue);
    } else if (inputNode->tag_name == "textarea") {
        // Replace text content of textarea
        inputNode->children.clear();
        auto textNode = std::make_unique<clever::html::SimpleNode>();
        textNode->type = clever::html::SimpleNode::Text;
        textNode->data = newValue;
        textNode->parent = inputNode;
        inputNode->children.push_back(std::move(textNode));
    }

    // Remove focus attribute and dispatch "change" and "blur" events via JS
    remove_attr(inputNode, [kBrowserFocusAttr UTF8String]);
    auto& jsEngine = [tab jsEngine];
    if (jsEngine) {
        clever::js::dispatch_event(jsEngine->context(), inputNode, "change");
        clever::js::dispatch_event(jsEngine->context(), inputNode, "blur");
    }

    // Serialize DOM and re-render
    auto& domTree = [tab domTree];
    if (domTree) {
        [tab currentHTML] = serialize_dom_node(domTree.get());
        [self doRender:[tab currentHTML]];
    }

    [tab setFocusedInputNode:nullptr];
}

- (void)renderView:(RenderView*)view didChangeInputValue:(NSString*)value {
    BrowserTab* tab = [self activeTab];
    if (!tab) return;
    auto* inputNode = [tab focusedInputNode];
    if (!inputNode) return;

    std::string newValue = value ? [value UTF8String] : "";

    // Update DOM value attribute live (for JS "input" event handlers)
    if (inputNode->tag_name == "input") {
        set_attr(inputNode, "value", newValue);
    } else if (inputNode->tag_name == "textarea") {
        inputNode->children.clear();
        auto textNode = std::make_unique<clever::html::SimpleNode>();
        textNode->type = clever::html::SimpleNode::Text;
        textNode->data = newValue;
        textNode->parent = inputNode;
        inputNode->children.push_back(std::move(textNode));
    }

    // Dispatch "input" event (not "change" — that's on commit)
    auto& jsEngine = [tab jsEngine];
    if (jsEngine) {
        clever::js::dispatch_event(jsEngine->context(), inputNode, "input");
    }
}

#pragma mark - CSS :hover Support

- (void)renderView:(RenderView*)view didMoveMouseAtX:(float)x y:(float)y {
    (void)view;
    BrowserTab* tab = [self activeTab];
    if (!tab) return;

    auto& elementRegions = [tab elementRegions];
    auto& domTree = [tab domTree];
    if (elementRegions.empty() || !domTree) return;

    // Hit-test to find the innermost element under the mouse
    clever::html::SimpleNode* target = nullptr;
    float smallest_area = std::numeric_limits<float>::max();
    for (auto it = elementRegions.rbegin(); it != elementRegions.rend(); ++it) {
        if (it->bounds.contains(x, y) && it->dom_node) {
            float area = it->bounds.width * it->bounds.height;
            if (area < smallest_area) {
                smallest_area = area;
                target = static_cast<clever::html::SimpleNode*>(it->dom_node);
            }
        }
    }

    // If hover target hasn't changed, do nothing
    if (target == _hoveredNode) return;

    // Dispatch JS mouse events for hover transitions
    auto& jsEngine = [tab jsEngine];
    bool domModifiedByHover = false;
    if (jsEngine) {
        clever::html::SimpleNode* oldTarget = _hoveredNode;

        // mouseout/mouseleave on old target
        if (oldTarget) {
            clever::js::dispatch_mouse_event(jsEngine->context(), oldTarget, "mouseout",
                x, y, x, y, 0, 0, false, false, false, false, 0);
            clever::js::dispatch_mouse_event(jsEngine->context(), oldTarget, "mouseleave",
                x, y, x, y, 0, 0, false, false, false, false, 0);
        }

        // mouseover/mouseenter on new target
        if (target) {
            clever::js::dispatch_mouse_event(jsEngine->context(), target, "mouseover",
                x, y, x, y, 0, 0, false, false, false, false, 0);
            clever::js::dispatch_mouse_event(jsEngine->context(), target, "mouseenter",
                x, y, x, y, 0, 0, false, false, false, false, 0);
        }

        // Also dispatch mousemove on the new target
        if (target) {
            clever::js::dispatch_mouse_event(jsEngine->context(), target, "mousemove",
                x, y, x, y, 0, 0, false, false, false, false, 0);
        }

        domModifiedByHover = clever::js::dom_was_modified(jsEngine->context());
    }

    if (_pageUsesHoverState) {
        // Remove ALL hover attributes from the DOM tree, then set new ones.
        // This is necessary because after re-render, the DOM tree is rebuilt
        // and old hover pointers become invalid.
        std::function<void(clever::html::SimpleNode*)> clearHover;
        clearHover = [&](clever::html::SimpleNode* n) {
            remove_attr(n, [kBrowserHoverAttr UTF8String]);
            for (auto& child : n->children) {
                if (child->type == clever::html::SimpleNode::Element)
                    clearHover(child.get());
            }
        };
        clearHover(domTree.get());
    }

    // Set hover on new chain (element + all ancestors)
    _hoveredNode = target;
    if (_pageUsesHoverState && target) {
        clever::html::SimpleNode* walk = target;
        while (walk) {
            set_attr(walk, [kBrowserHoverAttr UTF8String], "");
            walk = walk->parent;
        }
    }

    // Track last hover position for re-establishing after re-render
    _lastHoverX = x;
    _lastHoverY = y;

    if (!_pageUsesHoverState && !domModifiedByHover) {
        return;
    }

    // Debounced re-render: schedule a re-render in 140ms
    // Cancel any pending hover timer
    [_hoverTimer invalidate];
    _hoverTimer = [NSTimer scheduledTimerWithTimeInterval:0.14
                                                   target:self
                                                 selector:@selector(hoverTimerFired:)
                                                 userInfo:nil
                                                  repeats:NO];
}

- (void)hoverTimerFired:(NSTimer*)timer {
    (void)timer;
    _hoverTimer = nil;
    BrowserTab* tab = [self activeTab];
    if (!tab) return;
    auto& domTree = [tab domTree];
    if (!domTree) return;

    // --- Phase 1: Snapshot pixel regions of hovered elements before re-render ---
    // Build a map of element_id → pixel region snapshot from the current render
    std::unordered_map<std::string, PixelTransition> snapshots;
    auto& oldRegions = [tab elementRegions];
    RenderView* rv = tab.renderView;

    // Only snapshot if we have pixel data and a layout tree with transition info
    if (!rv || [rv basePixels].empty()) goto do_render;

    {
        const auto& basePx = [rv basePixels];
        int baseBw = [rv baseWidth];
        int baseBh = [rv baseHeight];

        // First, find which element IDs have CSS transitions from the OLD layout tree
        std::unordered_set<std::string> transitionIds;
        auto& oldLayout = [tab layoutRoot];
        if (oldLayout) {
            std::function<void(const clever::layout::LayoutNode&)> collectTransitionIds;
            collectTransitionIds = [&](const clever::layout::LayoutNode& n) {
                if (!n.element_id.empty() && n.transition_duration > 0) {
                    transitionIds.insert(n.element_id);
                }
                for (auto& c : n.children) collectTransitionIds(*c);
            };
            collectTransitionIds(*oldLayout);
        }

        // Snapshot pixel regions of elements with transition properties
        for (auto& region : oldRegions) {
            if (!region.dom_node) continue;
            auto* node = static_cast<clever::html::SimpleNode*>(region.dom_node);
            std::string eid;
            for (auto& [n, v] : node->attributes) {
                if (n == "id") { eid = v; break; }
            }
            if (eid.empty()) continue;
            if (!transitionIds.count(eid)) continue;
            if (snapshots.count(eid)) continue;

            // Extract pixel region from current buffer
            int x0 = static_cast<int>(region.bounds.x);
            int y0 = static_cast<int>(region.bounds.y);
            int w = static_cast<int>(region.bounds.width);
            int h = static_cast<int>(region.bounds.height);
            if (w <= 0 || h <= 0 || x0 < 0 || y0 < 0) continue;
            if (x0 + w > baseBw) w = baseBw - x0;
            if (y0 + h > baseBh) h = baseBh - y0;
            if (w <= 0 || h <= 0) continue;

            PixelTransition pt;
            pt.element_id = eid;
            pt.bounds = region.bounds;
            pt.width = w;
            pt.height = h;
            pt.from_pixels.resize(static_cast<size_t>(w) * h * 4);
            for (int row = 0; row < h; ++row) {
                size_t src_offset = (static_cast<size_t>(y0 + row) * baseBw + x0) * 4;
                size_t dst_offset = static_cast<size_t>(row) * w * 4;
                std::memcpy(&pt.from_pixels[dst_offset], &basePx[src_offset], w * 4);
            }
            snapshots[eid] = std::move(pt);
        }
    }

do_render:
    // --- Phase 2: Re-render with hover styles ---
    [tab currentHTML] = serialize_dom_node(domTree.get());
    _hoveredNode = nullptr;
    [self doRender:[tab currentHTML]];

    // --- Phase 3: Create pixel transitions for elements with CSS transition-duration > 0 ---
    if (!snapshots.empty()) {
        auto& layoutRoot = [tab layoutRoot];
        if (layoutRoot) {
            std::vector<PixelTransition> transitions;

            // Walk layout tree to find elements with transition-duration > 0
            std::function<void(const clever::layout::LayoutNode&)> findTransitions;
            findTransitions = [&](const clever::layout::LayoutNode& n) {
                if (!n.element_id.empty() && n.transition_duration > 0) {
                    auto it = snapshots.find(n.element_id);
                    if (it != snapshots.end()) {
                        PixelTransition pt = std::move(it->second);
                        pt.duration_s = n.transition_duration;
                        pt.delay_s = n.transition_delay;
                        pt.timing_function = n.transition_timing;
                        pt.bezier_x1 = n.transition_bezier_x1;
                        pt.bezier_y1 = n.transition_bezier_y1;
                        pt.bezier_x2 = n.transition_bezier_x2;
                        pt.bezier_y2 = n.transition_bezier_y2;
                        pt.steps_count = n.transition_steps_count;
                        transitions.push_back(std::move(pt));
                        snapshots.erase(it);
                    }
                }
                for (auto& c : n.children) findTransitions(*c);
            };
            findTransitions(*layoutRoot);

            if (!transitions.empty()) {
                [rv addPixelTransitions:std::move(transitions)];
            }
        }
    }

    // --- Phase 4: Re-establish hover state ---
    auto& newRegions = [tab elementRegions];
    auto& newDom = [tab domTree];
    if (!newDom || newRegions.empty()) return;

    clever::html::SimpleNode* newTarget = nullptr;
    float smallest = std::numeric_limits<float>::max();
    for (auto it = newRegions.rbegin(); it != newRegions.rend(); ++it) {
        if (it->bounds.contains(_lastHoverX, _lastHoverY) && it->dom_node) {
            float area = it->bounds.width * it->bounds.height;
            if (area < smallest) {
                smallest = area;
                newTarget = static_cast<clever::html::SimpleNode*>(it->dom_node);
            }
        }
    }
    _hoveredNode = newTarget;
}

#pragma mark - Context Menu & Double Click Event Dispatch

- (BOOL)renderView:(RenderView*)view didContextMenuAtX:(float)x y:(float)y {
    (void)view;
    BrowserTab* tab = [self activeTab];
    if (!tab) return NO;

    auto& jsEngine = [tab jsEngine];
    auto& elementRegions = [tab elementRegions];
    if (!jsEngine || elementRegions.empty()) return NO;

    // Hit-test to find the target element
    clever::html::SimpleNode* target = nullptr;
    float smallest_area = std::numeric_limits<float>::max();
    for (auto it = elementRegions.rbegin(); it != elementRegions.rend(); ++it) {
        if (it->bounds.contains(x, y) && it->dom_node) {
            float area = it->bounds.width * it->bounds.height;
            if (area < smallest_area) {
                smallest_area = area;
                target = static_cast<clever::html::SimpleNode*>(it->dom_node);
            }
        }
    }
    if (!target) return NO;

    // Dispatch "contextmenu" event with right-button (button=2)
    bool prevented = clever::js::dispatch_mouse_event(
        jsEngine->context(), target, "contextmenu",
        x, y, x, y, 2, 2, false, false, false, false, 0);

    return prevented ? YES : NO;
}

- (BOOL)renderView:(RenderView*)view didDoubleClickAtX:(float)x y:(float)y {
    (void)view;
    BrowserTab* tab = [self activeTab];
    if (!tab) return NO;

    auto& jsEngine = [tab jsEngine];
    auto& elementRegions = [tab elementRegions];
    if (!jsEngine || elementRegions.empty()) return NO;

    // Hit-test to find the target element
    clever::html::SimpleNode* target = nullptr;
    float smallest_area = std::numeric_limits<float>::max();
    for (auto it = elementRegions.rbegin(); it != elementRegions.rend(); ++it) {
        if (it->bounds.contains(x, y) && it->dom_node) {
            float area = it->bounds.width * it->bounds.height;
            if (area < smallest_area) {
                smallest_area = area;
                target = static_cast<clever::html::SimpleNode*>(it->dom_node);
            }
        }
    }
    if (!target) return NO;

    // Dispatch "dblclick" event (detail=2 for double-click)
    bool prevented = clever::js::dispatch_mouse_event(
        jsEngine->context(), target, "dblclick",
        x, y, x, y, 0, 0, false, false, false, false, 2);

    return prevented ? YES : NO;
}

#pragma mark - Keyboard Event Dispatch

- (BOOL)renderView:(RenderView*)view
    didKeyEvent:(NSString*)eventType
            key:(NSString*)key
           code:(NSString*)code
        keyCode:(int)keyCode
         repeat:(BOOL)isRepeat
        altKey:(BOOL)altKey
       ctrlKey:(BOOL)ctrlKey
       metaKey:(BOOL)metaKey
      shiftKey:(BOOL)shiftKey {
    (void)view;
    BrowserTab* tab = [self activeTab];
    if (!tab) return NO;

    auto& jsEngine = [tab jsEngine];
    auto& domTree = [tab domTree];
    if (!jsEngine || !domTree) return NO;

    // Determine target: focused input node, or fall back to <body>
    clever::html::SimpleNode* target = [tab focusedInputNode];
    if (!target) {
        target = domTree->find_element("body");
    }
    if (!target) {
        target = domTree.get(); // document root as last resort
    }

    // Build KeyboardEventInit
    clever::js::KeyboardEventInit init;
    init.key = key ? [key UTF8String] : "";
    init.code = code ? [code UTF8String] : "";
    init.key_code = keyCode;
    init.char_code = 0; // charCode is deprecated, typically 0 for keydown/keyup
    init.location = 0;  // DOM_KEY_LOCATION_STANDARD
    init.alt_key = altKey;
    init.ctrl_key = ctrlKey;
    init.meta_key = metaKey;
    init.shift_key = shiftKey;
    init.repeat = isRepeat;
    init.is_composing = false;

    std::string evtType = eventType ? [eventType UTF8String] : "keydown";

    bool prevented = clever::js::dispatch_keyboard_event(
        jsEngine->context(), target, evtType, init);

    return prevented ? YES : NO;
}

#pragma mark - Color Picker

- (void)colorPanelChanged:(NSColorPanel*)panel {
    BrowserTab* tab = [self activeTab];
    if (!tab) return;
    auto* inputNode = [tab focusedInputNode];
    if (!inputNode) return;
    if (get_attr(inputNode, "type") != "color") return;

    NSColor* color = [[panel color] colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    int r = static_cast<int>(color.redComponent * 255);
    int g = static_cast<int>(color.greenComponent * 255);
    int b = static_cast<int>(color.blueComponent * 255);
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02x%02x%02x", r, g, b);
    set_attr(inputNode, "value", std::string(hex));

    auto& jsEngine = [tab jsEngine];
    if (jsEngine) {
        clever::js::dispatch_event(jsEngine->context(), inputNode, "input");
    }

    auto& domTree = [tab domTree];
    if (domTree) {
        [tab currentHTML] = serialize_dom_node(domTree.get());
        [self doRender:[tab currentHTML]];
    }
}

#pragma mark - Find in Page

- (void)showFindBar {
    if (_findBarVisible) {
        // Toggle off
        [_findBar removeFromSuperview];
        _findBarVisible = NO;
        return;
    }

    if (!_findBar) {
        _findBar = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 400, 32)];
        _findBar.wantsLayer = YES;
        _findBar.layer.backgroundColor = [[NSColor controlBackgroundColor] CGColor];
        _findBar.layer.borderWidth = 1;
        _findBar.layer.borderColor = [[NSColor separatorColor] CGColor];
        _findBar.autoresizingMask = NSViewWidthSizable;

        NSTextField* label = [NSTextField labelWithString:@"Find:"];
        label.frame = NSMakeRect(8, 6, 36, 20);
        label.font = [NSFont systemFontOfSize:12];
        [_findBar addSubview:label];

        _findField = [[NSTextField alloc] initWithFrame:NSMakeRect(46, 4, 250, 24)];
        _findField.placeholderString = @"Search text...";
        _findField.bezelStyle = NSTextFieldRoundedBezel;
        _findField.font = [NSFont systemFontOfSize:12];
        _findField.delegate = self;
        _findField.autoresizingMask = NSViewWidthSizable;
        [_findBar addSubview:_findField];

        _findStatus = [NSTextField labelWithString:@""];
        _findStatus.frame = NSMakeRect(302, 6, 120, 20);
        _findStatus.font = [NSFont systemFontOfSize:11];
        _findStatus.textColor = [NSColor secondaryLabelColor];
        _findStatus.autoresizingMask = NSViewMinXMargin;
        [_findBar addSubview:_findStatus];

        NSButton* closeBtn = [NSButton buttonWithTitle:@"Done"
                                                target:self
                                                action:@selector(closeFindBar:)];
        closeBtn.bezelStyle = NSBezelStyleTexturedRounded;
        closeBtn.font = [NSFont systemFontOfSize:11];
        closeBtn.autoresizingMask = NSViewMinXMargin;
        [_findBar addSubview:closeBtn];

        // Position close button at right edge
        CGFloat barWidth = _contentArea.bounds.size.width;
        closeBtn.frame = NSMakeRect(barWidth - 60, 4, 52, 24);
        _findStatus.frame = NSMakeRect(barWidth - 180, 6, 110, 20);
    }

    // Position find bar at top of content area
    CGFloat contentH = _contentArea.bounds.size.height;
    _findBar.frame = NSMakeRect(0, contentH - 32,
                                _contentArea.bounds.size.width, 32);
    _findBar.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [_contentArea addSubview:_findBar];
    _findBarVisible = YES;

    // Focus the find field
    [self.window makeFirstResponder:_findField];
}

- (void)closeFindBar:(id)sender {
    (void)sender;
    [_findBar removeFromSuperview];
    _findBarVisible = NO;
}

#pragma mark - Zoom

- (void)zoomIn {
    BrowserTab* tab = [self activeTab];
    if (!tab) return;
    tab.renderView.pageScale = std::min(4.0, tab.renderView.pageScale + 0.25);
    [tab.renderView setNeedsDisplay:YES];
    [tab.renderView.window invalidateCursorRectsForView:tab.renderView];
}

- (void)zoomOut {
    BrowserTab* tab = [self activeTab];
    if (!tab) return;
    tab.renderView.pageScale = std::max(0.25, tab.renderView.pageScale - 0.25);
    [tab.renderView setNeedsDisplay:YES];
    [tab.renderView.window invalidateCursorRectsForView:tab.renderView];
}

- (void)zoomActualSize {
    BrowserTab* tab = [self activeTab];
    if (!tab) return;
    tab.renderView.pageScale = 1.0;
    [tab.renderView setNeedsDisplay:YES];
    [tab.renderView.window invalidateCursorRectsForView:tab.renderView];
}

- (void)saveScreenshot {
    NSSavePanel* panel = [NSSavePanel savePanel];
    panel.nameFieldStringValue = @"vibrowser_screenshot.png";
    panel.allowedContentTypes = @[[UTType typeWithIdentifier:@"public.png"]];
    panel.allowsOtherFileTypes = NO;

    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse result) {
        if (result != NSModalResponseOK || !panel.URL) return;

        // Capture the render view content as a bitmap
        BrowserTab* tab = [self activeTab];
        if (!tab || !tab.renderView) return;

        NSView* view = tab.renderView;
        NSBitmapImageRep* rep = [view bitmapImageRepForCachingDisplayInRect:view.bounds];
        if (!rep) return;
        [view cacheDisplayInRect:view.bounds toBitmapImageRep:rep];

        NSData* pngData = [rep representationUsingType:NSBitmapImageFileTypePNG
                                            properties:@{}];
        if (pngData) {
            [pngData writeToURL:panel.URL atomically:YES];
        }
    }];
}

- (void)printPage {
    BrowserTab* tab = [self activeTab];
    if (!tab || !tab.renderView) return;

    NSPrintInfo* printInfo = [[NSPrintInfo sharedPrintInfo] copy];

    // Configure print settings for landscape/portrait based on content aspect ratio
    [printInfo setHorizontalPagination:NSPrintingPaginationModeFit];
    [printInfo setVerticalPagination:NSPrintingPaginationModeFit];

    // Set reasonable margins
    [printInfo setTopMargin:36.0];
    [printInfo setBottomMargin:36.0];
    [printInfo setLeftMargin:36.0];
    [printInfo setRightMargin:36.0];

    NSPrintOperation* op = [NSPrintOperation printOperationWithView:tab.renderView
                                                          printInfo:printInfo];
    [op setShowsPrintPanel:YES];
    [op setShowsProgressPanel:YES];
    [op runOperationModalForWindow:self.window
                          delegate:nil
                    didRunSelector:nil
                       contextInfo:nil];
}

- (void)performFind {
    NSString* query = [_findField stringValue];
    if (query.length == 0) {
        [_findStatus setStringValue:@""];
        return;
    }

    // Search through the rendered text regions for matches
    BrowserTab* tab = [self activeTab];
    if (!tab) return;

    // Count occurrences in the current HTML source
    std::string html = [tab currentHTML];
    std::string q([query UTF8String]);

    int count = 0;
    size_t pos = 0;
    // Case-insensitive search in the source text
    std::string html_lower = html;
    std::string q_lower = q;
    std::transform(html_lower.begin(), html_lower.end(), html_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(q_lower.begin(), q_lower.end(), q_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    while ((pos = html_lower.find(q_lower, pos)) != std::string::npos) {
        count++;
        pos += q_lower.length();
    }

    if (count > 0) {
        [_findStatus setStringValue:[NSString stringWithFormat:@"%d found", count]];
        _findStatus.textColor = [NSColor labelColor];
    } else {
        [_findStatus setStringValue:@"Not found"];
        _findStatus.textColor = [NSColor systemRedColor];
    }
}


#pragma mark - Bookmarks

- (NSString*)bookmarksPlistPath {
    NSString* appSupport = [NSSearchPathForDirectoriesInDomains(
        NSApplicationSupportDirectory, NSUserDomainMask, YES) firstObject];
    NSString* vibrowserDir = [appSupport stringByAppendingPathComponent:@"Vibrowser"];
    return [vibrowserDir stringByAppendingPathComponent:@"bookmarks.plist"];
}

- (void)loadBookmarks {
    NSString* path = [self bookmarksPlistPath];
    NSArray* saved = [NSArray arrayWithContentsOfFile:path];
    if (!saved) {
        NSString* appSupport = [NSSearchPathForDirectoriesInDomains(
            NSApplicationSupportDirectory, NSUserDomainMask, YES) firstObject];
        NSString* legacyPath = [[appSupport stringByAppendingPathComponent:@"Clever"]
            stringByAppendingPathComponent:@"bookmarks.plist"];
        saved = [NSArray arrayWithContentsOfFile:legacyPath];
    }
    if (saved) {
        [_bookmarks removeAllObjects];
        for (NSDictionary* dict in saved) {
            if ([dict isKindOfClass:[NSDictionary class]] &&
                dict[@"title"] && dict[@"url"]) {
                [_bookmarks addObject:[dict mutableCopy]];
            }
        }
        [self saveBookmarks];
    }
}

- (void)saveBookmarks {
    NSString* path = [self bookmarksPlistPath];
    NSString* dir = [path stringByDeletingLastPathComponent];

    NSFileManager* fm = [NSFileManager defaultManager];
    if (![fm fileExistsAtPath:dir]) {
        [fm createDirectoryAtPath:dir
      withIntermediateDirectories:YES
                       attributes:nil
                            error:nil];
    }
    [_bookmarks writeToFile:path atomically:YES];
}

- (void)addBookmark {
    BrowserTab* tab = [self activeTab];
    if (!tab) return;

    NSString* url = tab.currentURL;
    NSString* title = tab.title;

    if (!url || url.length == 0) {
        NSBeep();
        return;
    }

    // Use URL as title fallback
    if (!title || title.length == 0 || [title isEqualToString:@"New Tab"]) {
        title = url;
    }

    // Check for duplicates
    for (NSDictionary* bm in _bookmarks) {
        if ([bm[@"url"] isEqualToString:url]) {
            NSBeep(); // Already bookmarked
            return;
        }
    }

    NSDictionary* bookmark = @{@"title": title, @"url": url};
    [_bookmarks addObject:bookmark];
    [self saveBookmarks];
    [self rebuildBookmarksMenu];

    // Visual feedback: briefly flash the window title
    NSString* originalTitle = self.window.title;
    self.window.title = [NSString stringWithFormat:@"Bookmarked: %@", title];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        self.window.title = originalTitle;
    });
}

- (void)removeBookmarkAtIndex:(NSInteger)index {
    if (index < 0 || index >= (NSInteger)_bookmarks.count) return;
    [_bookmarks removeObjectAtIndex:index];
    [self saveBookmarks];
    [self rebuildBookmarksMenu];
}

- (void)navigateToBookmark:(id)sender {
    NSMenuItem* item = (NSMenuItem*)sender;
    NSInteger index = item.tag;
    if (index < 0 || index >= (NSInteger)_bookmarks.count) return;

    NSDictionary* bookmark = _bookmarks[index];
    NSString* url = bookmark[@"url"];
    if (url && url.length > 0) {
        [self navigateToURL:url];
    }
}

- (void)rebuildBookmarksMenu {
    // Find the Bookmarks menu in the main menu bar
    NSMenu* mainMenu = [NSApp mainMenu];
    if (!mainMenu) return;

    NSMenuItem* bookmarksMenuItem = nil;
    for (NSMenuItem* item in mainMenu.itemArray) {
        if ([item.submenu.title isEqualToString:@"Bookmarks"]) {
            bookmarksMenuItem = item;
            break;
        }
    }
    if (!bookmarksMenuItem) return;

    NSMenu* bookmarksMenu = bookmarksMenuItem.submenu;

    // Remove all items after the separator (index 2 onwards: "Add Bookmark", separator, then bookmarks)
    while (bookmarksMenu.numberOfItems > 2) {
        [bookmarksMenu removeItemAtIndex:2];
    }

    // Add bookmark items
    for (NSInteger i = 0; i < (NSInteger)_bookmarks.count; i++) {
        NSDictionary* bm = _bookmarks[i];
        NSString* title = bm[@"title"];
        if (title.length > 40) {
            title = [[title substringToIndex:37] stringByAppendingString:@"..."];
        }

        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                      action:@selector(bookmarkMenuItemClicked:)
                                               keyEquivalent:@""];
        item.tag = i;
        [bookmarksMenu addItem:item];
    }

    // Add separator and "Remove All Bookmarks" if there are bookmarks
    if (_bookmarks.count > 0) {
        [bookmarksMenu addItem:[NSMenuItem separatorItem]];
        [bookmarksMenu addItemWithTitle:@"Remove All Bookmarks"
                                 action:@selector(removeAllBookmarksAction:)
                          keyEquivalent:@""];
    }
}

@end
