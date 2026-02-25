# Clever Engine Blueprint

C++20 browser engine. This document is the master architecture reference.
Each numbered section is self-contained enough to spawn focused implementation work.

---

## 0. Conventions

- **No marketing language.** Every section describes what to build and how.
- **Spec references** use short tags: `[HTML]` = WHATWG HTML Living Standard, `[CSS2]` = CSS 2.2, `[CSS3-x]` = CSS level 3 module x, `[DOM]` = WHATWG DOM, `[FETCH]` = WHATWG Fetch, `[URL]` = WHATWG URL, `[ECMA]` = ECMA-262.
- **LOC estimates** are rough, based on Ladybird (~1M LOC for 90% WPT) and Servo.
- **Deps** = external C/C++ libraries to link.
- **Phase** = implementation ordering (1 = first, 5 = last).

---

## 1. Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    Browser Process                       │
│  ┌──────────┐  ┌──────────┐  ┌────────┐  ┌───────────┐ │
│  │ UI Shell  │  │ Net Svc  │  │ Cookie │  │ Profile   │ │
│  │ (platform)│  │ (HTTP,TLS)│  │ Store  │  │ Storage   │ │
│  └──────────┘  └──────────┘  └────────┘  └───────────┘ │
│       │              │                                   │
│       │     IPC (message-passing, no shared mut state)   │
│       │              │                                   │
├───────┼──────────────┼───────────────────────────────────┤
│       ▼              ▼         Renderer Process (N)      │
│  ┌─────────────────────────────────────────────────────┐ │
│  │                                                     │ │
│  │   HTML Tokenizer → Tree Builder → DOM               │ │
│  │                                      │              │ │
│  │   CSS Tokenizer → Parser → CSSOM ────┤              │ │
│  │                                      ▼              │ │
│  │                              Style Resolution       │ │
│  │                              (parallel, per-subtree)│ │
│  │                                      │              │ │
│  │                                      ▼              │ │
│  │                              Layout Engine          │ │
│  │                              (parallel, per-subtree)│ │
│  │                                      │              │ │
│  │                                      ▼              │ │
│  │                              Paint → Display List   │ │
│  │                                      │              │ │
│  │   JS Engine (V8) ◄──── Web API Bindings             │ │
│  │                                                     │ │
│  └─────────────────────────────────────────────────────┘ │
│                         │                                │
├─────────────────────────┼────────────────────────────────┤
│                         ▼         GPU Process            │
│  ┌─────────────────────────────────────────────────────┐ │
│  │   Compositor (display list → GPU tiles → screen)    │ │
│  │   Skia backend                                      │ │
│  └─────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### Critical Design Rules (learned from Chromium/Servo/Ladybird)

1. **Immutable data between pipeline stages.** Chromium's biggest mistake was mutable shared DOM/style data accessed by multiple pipeline stages simultaneously. Each stage produces an immutable snapshot consumed by the next. Mutations go through a controlled commit path that invalidates downstream caches.

2. **Message-passing between processes.** No shared memory for mutable state. IPC uses serialized messages (like Mojo in Chromium, but simpler). Renderer never directly touches network or disk.

3. **Parallel style and layout.** Servo proved 2-5x speedups via Rayon work-stealing parallelism for style resolution and layout. We use the same approach: independent subtrees are processed in parallel.

4. **Pipeline stages do not interleave.** Style resolution completes before layout begins. Layout completes before paint begins. No mid-pipeline JS execution except at well-defined yield points.

5. **JS engine is embedded, not built.** Building a JS engine is a multi-team-year project on its own. We embed V8 (or SpiderMonkey as fallback). Web API bindings are generated from WebIDL specs.

---

## 2. Module Map

| # | Module | Dir | Phase | Est. LOC | Deps |
|---|--------|-----|-------|----------|------|
| 2.1 | Platform | `src/platform/` | 1 | 8K | OS APIs |
| 2.2 | IPC | `src/ipc/` | 1 | 5K | Platform |
| 2.3 | URL | `src/url/` | 1 | 3K | ICU |
| 2.4 | Networking | `src/net/` | 1 | 25K | libcurl or custom, OpenSSL/BoringSSL |
| 2.5 | HTML Parser | `src/html/` | 2 | 15K | — |
| 2.6 | DOM | `src/dom/` | 2 | 30K | — |
| 2.7 | CSS Parser | `src/css/parser/` | 2 | 12K | — |
| 2.8 | CSS Style Engine | `src/css/style/` | 2 | 25K | — |
| 2.9 | Layout | `src/layout/` | 3 | 40K | — |
| 2.10 | Text | `src/text/` | 2 | 10K | HarfBuzz, FreeType, ICU |
| 2.11 | Image | `src/image/` | 2 | 5K | libpng, libjpeg-turbo, libwebp |
| 2.12 | Paint | `src/paint/` | 3 | 15K | — |
| 2.13 | Compositor | `src/compositor/` | 3 | 20K | Skia |
| 2.14 | JS Integration | `src/js/` | 3 | 20K | V8 |
| 2.15 | Web API Bindings | `src/bindings/` | 3 | 40K (generated) | V8, WebIDL codegen |
| 2.16 | Storage | `src/storage/` | 4 | 8K | SQLite |
| 2.17 | DevTools | `src/devtools/` | 5 | 15K | WebSocket lib |
| 2.18 | UI Shell | `src/shell/` | 4 | 10K | Platform windowing |
| 2.19 | Build/Infra | `cmake/`, `tools/` | 1 | 3K | CMake, vcpkg |

**Total estimate: ~310K LOC** for core engine (excluding V8/Skia/deps).
With generated bindings: ~350K.
Full "90% web" target with tests: ~500K.

---

## 3. Module Specifications

Each module below contains: purpose, spec scope, key types, public API surface, internal architecture notes, dependencies, and a prompt template.

---

### 3.1 Platform Abstraction (`src/platform/`)

**Phase 1 | ~8K LOC**

**Purpose:** Abstract OS-specific APIs behind stable interfaces. Threading, file I/O, timers, memory-mapped files, windowing hooks.

**Key types:**

```cpp
namespace clever::platform {

class Thread;                    // std::jthread wrapper with naming
class ThreadPool;                // Work-stealing pool (like Rayon)
class TaskRunner;                // Post tasks to specific threads
class EventLoop;                 // Per-thread message pump
class Timer;                     // One-shot and repeating
class File;                      // Async file I/O
class SharedMemoryRegion;        // For compositor buffers
class Window;                    // Native window handle + events

enum class ThreadType {
    Main, IO, Renderer, Compositor, Worker
};

}
```

**Architecture:**

- Main thread runs the browser-process event loop
- IO thread handles all network and disk I/O
- Renderer threads (one per tab/renderer process) run the HTML→paint pipeline
- Compositor thread handles GPU submission
- Worker threads form a pool for parallel style/layout

**Deps:** POSIX/Win32/Cocoa APIs, `std::jthread`, `std::atomic`, `std::latch`/`std::barrier` (C++20)

**Prompt template:**
> Implement the platform abstraction layer for Clever engine in C++20.
> Target macOS first (Cocoa/POSIX), with Linux (X11/Wayland) as secondary.
> Key deliverables: ThreadPool with work-stealing (similar to Rayon),
> EventLoop with message posting, Timer (one-shot + repeating),
> async File I/O, Window abstraction for native windows.
> Use std::jthread, std::atomic, C++20 concurrency primitives.
> No external threading libraries. Tests for each component.

---

### 3.2 IPC (`src/ipc/`)

**Phase 1 | ~5K LOC**

**Purpose:** Inter-process communication between browser process, renderer processes, and GPU process. Serialized message-passing only — no shared mutable state.

**Key types:**

```cpp
namespace clever::ipc {

class MessagePipe;        // Bidirectional byte pipe (Unix domain socket / named pipe)
class MessageChannel;     // Typed message send/receive over a pipe
class Serializer;         // POD + string + vector serialization
class Deserializer;

// Message envelope
struct Message {
    uint32_t type;
    uint32_t request_id;
    std::vector<uint8_t> payload;
};

// Shared memory for large buffers (compositor tiles)
class SharedBuffer;

}
```

**Architecture:**

- Browser↔Renderer: request/response for navigation, resource loading
- Renderer→GPU: display list submission
- All messages are serialized. No raw pointers cross process boundaries.
- Each message type gets a generated serialize/deserialize pair (from IDL or manual)

**Prompt template:**
> Implement IPC layer for Clever engine multi-process architecture.
> Use Unix domain sockets on POSIX, with fallback design for named pipes on Windows.
> Provide MessagePipe (raw bytes), MessageChannel (typed messages with
> serialize/deserialize), and SharedBuffer (mmap-backed shared memory for
> compositor tile data). Messages are fire-and-forget or request/response
> (matched by request_id). Include message dispatch loop integration with
> platform::EventLoop. Tests for serialization round-trips, pipe communication
> between forked processes, and SharedBuffer lifecycle.

---

### 3.3 URL Parser (`src/url/`)

**Phase 1 | ~3K LOC**

**Purpose:** WHATWG URL Standard compliant parser. Every URL in the engine flows through this.

**Spec scope:** `[URL]` — full WHATWG URL Standard including:

- URL parsing algorithm (scheme, authority, path, query, fragment)
- Percent-encoding
- IDNA (internationalized domain names) via ICU
- Origin computation
- URL serialization
- Relative URL resolution
- `blob:` and `data:` URL schemes

**Key types:**

```cpp
namespace clever::url {

struct URL {
    std::string scheme;
    std::string username;
    std::string password;
    std::string host;         // after IDNA processing
    std::optional<uint16_t> port;
    std::string path;
    std::string query;
    std::string fragment;

    std::string serialize() const;
    std::string origin() const;
    bool is_special() const;  // http, https, ftp, ws, wss, file
};

std::optional<URL> parse(std::string_view input, const URL* base = nullptr);
bool urls_same_origin(const URL& a, const URL& b);

}
```

**Deps:** ICU (for IDNA)

**Prompt template:**
> Implement WHATWG URL Standard parser in C++20.
> Must handle: all special schemes (http/https/ftp/ws/wss/file), percent-encoding,
> IDNA via ICU, relative URL resolution with base URL, origin computation,
> data: and blob: URL recognition. Use the WHATWG URL parsing state machine
> from the spec. Output: clever::url::URL struct with serialize(), origin().
> Test against WPT url/ test fixtures (provide as JSON test data).

---

### 3.4 Networking (`src/net/`)

**Phase 1 | ~25K LOC**

**Purpose:** HTTP/1.1, HTTP/2, TLS, cookie management, CORS, CSP, Fetch algorithm.

**Spec scope:**

- `[FETCH]` — WHATWG Fetch Standard (request/response lifecycle, CORS, redirect handling)
- HTTP/1.1 (RFC 7230-7235), HTTP/2 (RFC 7540)
- TLS 1.2/1.3 via BoringSSL
- Cookies (RFC 6265bis)
- Content-Security-Policy Level 3
- HSTS, certificate validation

**Key types:**

```cpp
namespace clever::net {

enum class Method { GET, POST, PUT, DELETE, HEAD, OPTIONS, PATCH };

struct Request {
    url::URL url;
    Method method = Method::GET;
    HeaderMap headers;
    std::vector<uint8_t> body;
    RequestMode mode;          // cors, no-cors, same-origin, navigate
    CredentialsMode credentials;
    ReferrerPolicy referrer_policy;
    std::optional<url::URL> referrer;
};

struct Response {
    uint16_t status;
    HeaderMap headers;
    std::vector<uint8_t> body;
    ResponseType type;         // basic, cors, opaque, error
    url::URL url;
    bool was_redirected = false;
};

class NetworkService {
public:
    // Async fetch — callback on IO thread, caller posts to renderer
    void fetch(Request request, FetchCallback callback);
    void cancel(uint64_t request_id);

    CookieStore& cookies();
    HSTSStore& hsts();
};

class CookieStore;    // RFC 6265bis, SameSite, Secure, HttpOnly
class HSTSStore;      // HSTS preload list + learned entries
class CORSChecker;    // Preflight cache + actual CORS check
class CSPChecker;     // CSP directive evaluation

}
```

**Architecture:**

- `NetworkService` lives in the browser process
- Renderer sends fetch requests via IPC
- Connection pooling per origin (6 connections HTTP/1.1, 1 connection HTTP/2 multiplexed)
- TLS session resumption
- Redirect chain limit: 20
- Response streaming for large bodies

**Deps:** BoringSSL (or OpenSSL), nghttp2 (HTTP/2 framing), c-ares (async DNS)

**Prompt template:**
> Implement the Clever engine networking stack in C++20.
> Sub-modules: HTTP client (HTTP/1.1 + HTTP/2 via nghttp2), TLS via BoringSSL,
> CookieStore (RFC 6265bis with SameSite/Secure/HttpOnly), CORS preflight
> and actual request checking per WHATWG Fetch, HSTS with preload list,
> CSP Level 3 directive evaluation, connection pooling (6/origin for H1,
> multiplexed H2). NetworkService runs on IO thread, receives requests via
> message queue, calls back with Response. Streaming support for bodies >1MB.
> Redirect chain limit 20. Tests: unit tests for each sub-module, integration
> test with local HTTP server fixture.

---

### 3.5 HTML Parser (`src/html/`)

**Phase 2 | ~15K LOC**

**Purpose:** WHATWG HTML tokenizer and tree builder. Produces a DOM tree.

**Spec scope:** `[HTML]` Section 13 — Parsing HTML documents:

- Tokenizer state machine (all 80 states)
- Tree construction (all insertion modes)
- Error recovery per spec (not ad-hoc)
- `<template>` support
- Foreign content (SVG, MathML) — basic
- Fragment parsing (for innerHTML)
- Character encoding detection (BOM, meta charset, HTTP header)

**Key types:**

```cpp
namespace clever::html {

// Tokenizer output
struct Token {
    enum Type { DOCTYPE, StartTag, EndTag, Character, Comment, EndOfFile };
    Type type;
    std::string name;
    std::vector<Attribute> attributes;
    bool self_closing = false;
    std::string data;  // for Character/Comment
};

class Tokenizer {
public:
    explicit Tokenizer(std::string_view input);
    Token next_token();
    void set_state(State state);  // for tree builder feedback
};

class TreeBuilder {
public:
    explicit TreeBuilder(dom::Document& document);
    void process_token(const Token& token);
    // Implements all insertion modes from the spec
};

dom::Document parse(std::string_view html);
dom::DocumentFragment parse_fragment(std::string_view html, dom::Element& context);

}
```

**Architecture:**

- Tokenizer is a state machine, yields one token at a time
- Tree builder consumes tokens, builds DOM nodes, manages the stack of open elements
- Tree builder can switch tokenizer state (e.g., for `<script>`, `<style>`, `<textarea>`)
- Character encoding: try BOM → HTTP header → meta charset → prescan → fallback UTF-8
- Error handling: per-spec recovery (the spec defines exact behavior for every error case)

**Prompt template:**
> Implement WHATWG HTML parser for Clever engine in C++20.
> Two components: Tokenizer (all 80 states from spec section 13.2.5) and
> TreeBuilder (all insertion modes from section 13.2.6). Tokenizer yields
> Token structs (DOCTYPE, StartTag, EndTag, Character, Comment, EOF).
> TreeBuilder produces dom::Document by processing tokens.
> Must handle: foster parenting, adoption agency algorithm, formatting elements,
> template element, fragment parsing (innerHTML), script-supporting elements
> (mark for later — no script execution yet). Encoding detection: BOM, then
> meta prescan, then UTF-8 default. Test against html5lib-tests tree
> construction test fixtures.

---

### 3.6 DOM (`src/dom/`)

**Phase 2 | ~30K LOC**

**Purpose:** Document Object Model — the in-memory tree that represents the page.

**Spec scope:**

- `[DOM]` — WHATWG DOM Standard (Node, Element, Document, Text, Comment, DocumentFragment, Attr)
- `[HTML]` — HTMLElement subclasses (~30 elements for "90% web")
- Event dispatch (capture, target, bubble phases)
- MutationObserver (basic)
- TreeWalker/NodeIterator
- ParentNode/ChildNode mixins (querySelector, append, prepend, etc.)
- classList, dataset
- innerHTML/outerHTML (via fragment parser)

**"90% web" HTML elements (30):**

```
html, head, body, div, span, p, a, img, ul, ol, li,
h1-h6, table, tr, td, th, thead, tbody,
form, input, button, textarea, select, option, label,
script, style, link, meta, title, br, hr
```

**Key types:**

```cpp
namespace clever::dom {

class Node {
public:
    enum Type { Element, Text, Comment, Document, DocumentFragment, DocumentType };
    Type node_type() const;
    Node* parent() const;
    Node* first_child() const;
    Node* last_child() const;
    Node* next_sibling() const;
    Node* previous_sibling() const;

    Node& append_child(std::unique_ptr<Node> child);
    Node& insert_before(std::unique_ptr<Node> child, Node* reference);
    std::unique_ptr<Node> remove_child(Node& child);

    // Invalidation — notifies style/layout that this subtree changed
    void mark_dirty(DirtyFlags flags);
};

class Element : public Node {
public:
    const std::string& tag_name() const;
    const std::string& namespace_uri() const;
    std::optional<std::string> get_attribute(std::string_view name) const;
    void set_attribute(const std::string& name, const std::string& value);
    void remove_attribute(const std::string& name);
    const std::string& id() const;
    const ClassList& class_list() const;
    // querySelector, querySelectorAll — delegate to selector matching
};

class Document : public Node {
public:
    Element* document_element() const; // <html>
    Element* body() const;
    Element* head() const;
    Element* get_element_by_id(std::string_view id);
    NodeList get_elements_by_tag_name(std::string_view tag);
    NodeList get_elements_by_class_name(std::string_view class_name);
    Element& create_element(const std::string& tag);
    Text& create_text_node(const std::string& data);
    // Style sheets attached to this document
    std::vector<css::StyleSheet*> style_sheets;
};

class Text : public Node { /* character data */ };
class Comment : public Node { /* comment data */ };

// Events
class Event;
class EventTarget;

}
```

**Architecture:**

- Nodes own their children via `unique_ptr` (no reference counting for tree edges)
- Parent pointers are raw (non-owning)
- Each node carries `DirtyFlags` for incremental style/layout invalidation
- Attribute storage: small vector (most elements have <5 attributes)
- Event dispatch follows DOM spec (capture → target → bubble)
- `innerHTML` setter calls fragment parser and replaces children

**Mutation invalidation protocol:**

- Attribute change → mark style dirty on element
- Child add/remove → mark layout dirty on parent
- Text content change → mark layout dirty on parent
- Dirty flags propagate up to document root via `mark_dirty()`
- Style/layout only recompute dirty subtrees

**Prompt template:**
> Implement the DOM module for Clever engine in C++20.
> Core types: Node (base), Element, Text, Comment, Document, DocumentFragment.
> Nodes own children via unique_ptr, parent is raw pointer.
> Element has attribute storage (small_vector<Attribute, 4>), id() shortcut,
> class_list(), get/set/remove_attribute.
> Document has get_element_by_id (backed by id→Element* hash map updated on
> attribute mutations), style_sheets vector.
> Event dispatch: EventTarget mixin with addEventListener/removeEventListener/
> dispatchEvent. Capture → target → bubble phases per DOM spec.
> Mutation invalidation: DirtyFlags enum (Style, Layout, Paint) with
> mark_dirty() propagating up the tree.
> Implement querySelector/querySelectorAll on Element and Document (using
> selector matching from css module — stub the interface for now).
> Test: tree construction, attribute mutation, event dispatch phases,
> id map updates, dirty flag propagation.

---

### 3.7 CSS Parser (`src/css/parser/`)

**Phase 2 | ~12K LOC**

**Purpose:** CSS tokenizer and parser per CSS Syntax Level 3.

**Spec scope:** `[CSS3-syntax]` CSS Syntax Module Level 3:

- Tokenization (all token types)
- Component value parsing
- Rule parsing (style rules, at-rules: @media, @import, @font-face, @keyframes)
- Selector parsing `[CSS3-selectors]` Selectors Level 4 (subset)
- Property value parsing for supported properties

**Supported selector subset (covers ~95% of real-world usage):**

```
Type selectors:          div, p, span
Class selectors:         .foo
ID selectors:            #bar
Attribute selectors:     [attr], [attr=val], [attr~=val], [attr|=val],
                         [attr^=val], [attr$=val], [attr*=val]
Pseudo-classes:          :hover, :focus, :active, :visited, :first-child,
                         :last-child, :nth-child(), :not(), :root, :empty,
                         :enabled, :disabled, :checked
Pseudo-elements:         ::before, ::after, ::first-line, ::first-letter,
                         ::placeholder
Combinators:             descendant ( ), child (>), adjacent (+), general (~)
Selector lists:          a, b, c
```

**Key types:**

```cpp
namespace clever::css {

// Tokenizer
struct CSSToken {
    enum Type {
        Ident, Function, AtKeyword, Hash, String, Number, Percentage,
        Dimension, Whitespace, Colon, Semicolon, Comma, LeftBrace,
        RightBrace, LeftParen, RightParen, LeftBracket, RightBracket,
        Delim, CDC, CDO, EndOfFile
    };
    Type type;
    std::string value;
    double numeric_value = 0;
    std::string unit; // for Dimension
};

class CSSTokenizer {
public:
    explicit CSSTokenizer(std::string_view input);
    CSSToken next_token();
};

// Parsed structures
struct Selector;       // compound selector chain with combinators
struct Declaration;    // property: value !important
struct StyleRule;      // selector list + declarations
struct MediaQuery;
struct KeyframesRule;
struct ImportRule;

struct StyleSheet {
    std::vector<StyleRule> rules;
    std::vector<ImportRule> imports;
    std::vector<MediaQuery> media_queries;
    std::vector<KeyframesRule> keyframes;
};

StyleSheet parse_stylesheet(std::string_view css);
std::vector<Declaration> parse_declaration_block(std::string_view css);
Selector parse_selector(std::string_view selector);

}
```

**Prompt template:**
> Implement CSS parser for Clever engine in C++20, per CSS Syntax Level 3.
> Tokenizer: all token types from spec section 4. Parser: stylesheet parsing
> (style rules, @media, @import, @font-face, @keyframes), declaration block
> parsing, selector parsing (Selectors Level 4 subset — see supported list).
> Selector representation: CompoundSelector (sequence of simple selectors)
> linked by Combinator into ComplexSelector chains. StyleRule holds selector
> list + vector<Declaration>. Declaration holds property name, list of
> component values, important flag. Test against CSS parsing test fixtures.

---

### 3.8 CSS Style Engine (`src/css/style/`)

**Phase 2 | ~25K LOC**

**Purpose:** Selector matching, cascade, specificity, computed values. Produces a `ComputedStyle` for every element.

**Spec scope:**

- `[CSS3-cascade]` CSS Cascading and Inheritance Level 4 (cascade order, specificity, inheritance)
- `[CSS3-values]` CSS Values and Units Level 4 (lengths, percentages, calc(), colors)
- Supported properties (~80 properties covering "90% web"):

**Supported CSS properties (80):**

```
/* Box model */
display, position, float, clear, box-sizing,
width, height, min-width, max-width, min-height, max-height,
margin[-top/right/bottom/left], padding[-top/right/bottom/left],
border[-top/right/bottom/left][-width/-style/-color], border-radius,

/* Flexbox */
flex-direction, flex-wrap, justify-content, align-items, align-self,
align-content, flex-grow, flex-shrink, flex-basis, order, gap,

/* Positioning */
top, right, bottom, left, z-index,

/* Text */
color, font-family, font-size, font-weight, font-style,
line-height, text-align, text-decoration, text-transform,
text-indent, letter-spacing, word-spacing, white-space,
text-overflow, word-break, overflow-wrap,

/* Visual */
background-color, background-image, background-position, background-size,
background-repeat, opacity, visibility, overflow[-x/-y],
box-shadow, outline[-width/-style/-color],

/* Table */
table-layout, border-collapse, border-spacing,

/* List */
list-style-type, list-style-position,

/* Transition/Animation */
transition[-property/-duration/-timing-function/-delay],
animation[-name/-duration/-timing-function/-delay/-iteration-count/
          -direction/-fill-mode/-play-state],

/* Misc */
cursor, pointer-events, content (for ::before/::after),
transform, vertical-align
```

**Key types:**

```cpp
namespace clever::css {

struct ComputedStyle {
    // Every supported property has a typed field
    Display display = Display::Inline;
    Position position = Position::Static;
    // ... ~80 properties
    Color color;
    Length font_size;
    FontWeight font_weight;
    // etc.
};

class StyleResolver {
public:
    // Resolve styles for entire document (parallelizable per subtree)
    void resolve(dom::Document& document, const std::vector<StyleSheet>& sheets);

    // Resolve single element (for incremental updates)
    ComputedStyle resolve_element(const dom::Element& element,
                                  const ComputedStyle& parent_style);
};

class SelectorMatcher {
public:
    bool matches(const dom::Element& element, const Selector& selector) const;
    Specificity specificity(const Selector& selector) const;
};

class PropertyCascade {
public:
    // Collect all declarations matching an element, sort by cascade order
    ComputedStyle cascade(const dom::Element& element,
                          const std::vector<MatchedRule>& matched_rules,
                          const ComputedStyle& parent);
};

}
```

**Architecture:**

- **Bloom filter** on ancestor classes/ids/tags for fast selector rejection (Servo technique)
- **Rule hash map** indexed by rightmost simple selector type (id, class, tag) for O(1) candidate lookup
- **Parallel style resolution:** traverse DOM tree, process independent subtrees on worker threads. Each element's ComputedStyle depends only on parent's ComputedStyle and matched rules.
- **Specificity:** (inline, id-count, class-count, type-count) per Selectors spec
- **Inheritance:** properties marked as inherited copy from parent ComputedStyle
- **Initial values:** per-property defaults from spec
- **Value resolution:** parse-time values → specified → computed (resolve relative units, calc(), currentColor, inherit, initial, unset, revert)

**Prompt template:**
> Implement CSS style engine for Clever engine in C++20.
> Components: SelectorMatcher (match element against selector, compute specificity),
> PropertyCascade (collect matched rules, sort by cascade order — origin, specificity,
> order — and produce ComputedStyle), StyleResolver (drive full-document style
> resolution with parallel subtree processing).
> Optimizations: Bloom filter on ancestor chain for fast selector rejection,
> rule hash map indexed by rightmost selector type.
> ComputedStyle struct with ~80 typed property fields.
> Property value resolution: specified → computed (resolve em/rem/% to px,
> resolve inherit/initial/unset, resolve calc(), resolve currentColor).
> Inheritance: mark inheritable properties, copy from parent ComputedStyle.
> Test: selector matching, specificity calculation, cascade ordering,
> inheritance chain, value resolution (em, rem, %, calc).

---

### 3.9 Layout Engine (`src/layout/`)

**Phase 3 | ~40K LOC**

**Purpose:** Compute geometry (position and size) for every box. Consumes ComputedStyle, produces LayoutTree with coordinates.

**Spec scope:**

- `[CSS2]` Visual formatting model (block formatting context, inline formatting context, positioning)
- `[CSS3-flexbox]` CSS Flexible Box Layout Level 1
- `[CSS3-box]` CSS Box Model Level 3
- `[CSS3-sizing]` CSS Box Sizing Level 3 (intrinsic sizes, fit-content)
- Table layout (basic — fixed and auto)

**Layout modes:**

```
1. Block flow layout (BFC)
2. Inline flow layout (IFC) with line breaking
3. Flexbox layout
4. Table layout (fixed + auto)
5. Positioned layout (relative, absolute, fixed, sticky)
6. Float layout
7. Replaced element layout (img, input, etc.)
```

**Key types:**

```cpp
namespace clever::layout {

struct BoxGeometry {
    float x, y;                           // position relative to containing block
    float width, height;                  // content box size
    EdgeSizes margin, border, padding;    // all four sides
    float scroll_width, scroll_height;    // for overflow
};

class LayoutNode {
public:
    const dom::Node* dom_node() const;
    const css::ComputedStyle& style() const;
    BoxGeometry& geometry();

    // Children
    const std::vector<std::unique_ptr<LayoutNode>>& children() const;

    // Layout mode this node establishes
    LayoutMode layout_mode() const;
};

class LayoutTree {
public:
    LayoutNode* root() const;

    // Build layout tree from DOM + computed styles
    static LayoutTree build(const dom::Document& doc);

    // Compute layout (parallelizable per independent formatting context)
    void compute(float viewport_width, float viewport_height);
};

// Individual layout algorithms
class BlockFormattingContext;
class InlineFormattingContext;
class FlexFormattingContext;
class TableFormattingContext;
class PositionedLayout;

// Line breaking
class LineBreaker {
public:
    std::vector<LineBox> break_into_lines(
        const std::vector<InlineItem>& items,
        float available_width);
};

}
```

**Architecture:**

- **Layout tree construction:** Walk DOM, skip `display: none`, generate anonymous boxes for inline-in-block and block-in-inline, generate boxes for `::before`/`::after`.
- **Two-pass layout:**
  1. **Constraint pass (top-down):** propagate available width/height from parent to children
  2. **Size pass (bottom-up):** compute intrinsic sizes from children, resolve auto sizes
- **Parallel layout:** Independent formatting contexts can be laid out on separate threads. A BFC that doesn't interact with floats from parent is independent.
- **Line breaking:** ICU for Unicode line break opportunities, hyphenation optional (Phase 5)
- **Fragmentation:** Not in initial scope (no multi-column, no paged media)

**Chromium lesson:** Tables are the hardest layout mode. Do fixed table layout first (trivial), add auto table layout later.

**Prompt template:**
> Implement layout engine for Clever engine in C++20.
> Layout tree built from DOM + ComputedStyle. Each LayoutNode has BoxGeometry
> (x, y, width, height, margin/border/padding edges).
> Layout modes: BlockFormattingContext (block flow, margin collapsing, auto
> height from children), InlineFormattingContext (line boxes, inline items,
> line breaking via ICU, vertical alignment), FlexFormattingContext (CSS
> Flexbox Level 1 — main axis, cross axis, flex-grow/shrink/basis, wrapping,
> alignment), PositionedLayout (relative=offset, absolute=positioned to
> containing block, fixed=positioned to viewport, sticky=clamp within
> scroll bounds), FloatLayout (left/right floats, clearance).
> Table layout: fixed mode only initially.
> Two-pass: constraints top-down, sizes bottom-up.
> Parallel: independent formatting contexts can be computed on worker threads.
> Tests: block layout, inline wrapping, flexbox (growth, shrink, wrap,
> alignment), positioned elements, float clearing, margin collapsing.

---

### 3.10 Text Shaping and Rendering (`src/text/`)

**Phase 2 | ~10K LOC**

**Purpose:** Shape text runs into positioned glyphs. Handle fonts, fallback, BiDi.

**Spec scope:**

- Unicode BiDi Algorithm (UAX #9) — basic LTR/RTL
- Unicode line breaking (UAX #14)
- Font matching (CSS Fonts Level 4 subset)
- Glyph shaping via HarfBuzz
- Font rasterization via FreeType

**Key types:**

```cpp
namespace clever::text {

struct ShapedGlyph {
    uint32_t glyph_id;
    float x_advance, y_advance;
    float x_offset, y_offset;
    uint32_t cluster;  // maps back to source character index
};

struct ShapedRun {
    std::vector<ShapedGlyph> glyphs;
    float total_advance;
    float ascent, descent;
    Font* font;
};

class TextShaper {
public:
    ShapedRun shape(std::string_view text, const Font& font,
                    float font_size, TextDirection direction);
};

class FontDatabase {
public:
    Font* match(const css::FontDescriptor& descriptor);
    void load_system_fonts();
    void load_web_font(const std::string& family, std::span<const uint8_t> data);
};

class Font {
public:
    float ascent(float size) const;
    float descent(float size) const;
    float line_gap(float size) const;
    // FreeType face handle
};

}
```

**Deps:** HarfBuzz (shaping), FreeType (rasterization), ICU (BiDi, line breaking), fontconfig (Linux font discovery)

**Prompt template:**
> Implement text shaping module for Clever engine in C++20.
> FontDatabase loads system fonts (CoreText on macOS, fontconfig on Linux),
> matches CSS font descriptors (family, weight, style, stretch) to best
> available font with fallback chain.
> TextShaper uses HarfBuzz for glyph shaping — input: UTF-8 text + Font +
> size + direction, output: ShapedRun with positioned glyphs.
> BiDi: use ICU ubidi for paragraph-level BiDi reordering.
> Line breaking: use ICU ubrk for Unicode line break opportunities.
> Font metrics (ascent, descent, line gap) from FreeType face.
> Web font loading: parse TTF/OTF/WOFF2 into FreeType face.
> Tests: basic Latin shaping, Arabic shaping (joining), CJK, emoji,
> font fallback when primary font missing glyph.

---

### 3.11 Image Decoding (`src/image/`)

**Phase 2 | ~5K LOC**

**Purpose:** Decode image formats into pixel buffers.

**Supported formats:**

- PNG (via libpng)
- JPEG (via libjpeg-turbo)
- GIF (via giflib — first frame only, animation later)
- WebP (via libwebp)
- SVG (Phase 5 — defer)
- ICO/BMP (via custom decoder, trivial)

**Key types:**

```cpp
namespace clever::image {

struct DecodedImage {
    uint32_t width, height;
    std::vector<uint8_t> pixels;  // RGBA8888
    bool has_alpha;
};

std::optional<DecodedImage> decode(std::span<const uint8_t> data,
                                    std::string_view mime_type);

// Async decode on worker thread
void decode_async(std::span<const uint8_t> data,
                  std::string_view mime_type,
                  std::function<void(std::optional<DecodedImage>)> callback);

}
```

**Prompt template:**
> Implement image decoding for Clever engine in C++20.
> Support PNG (libpng), JPEG (libjpeg-turbo), WebP (libwebp), GIF (giflib,
> first frame only). All outputs as RGBA8888 pixel buffers.
> Sync decode() and async decode_async() (runs on platform::ThreadPool).
> MIME type detection: try magic bytes if mime_type is empty.
> Tests: decode a 1x1 PNG, decode a JPEG, decode with invalid data (returns
> nullopt), async decode completion.

---

### 3.12 Paint (`src/paint/`)

**Phase 3 | ~15K LOC**

**Purpose:** Walk the layout tree and produce a display list of paint commands.

**Key types:**

```cpp
namespace clever::paint {

struct PaintCommand {
    enum Type {
        FillRect, StrokeRect, DrawText, DrawImage, PushClip, PopClip,
        PushOpacity, PopOpacity, PushTransform, PopTransform,
        DrawBoxShadow, DrawBorder, DrawBackground
    };
    Type type;
    // Union/variant of command data
};

class DisplayList {
public:
    void add(PaintCommand cmd);
    const std::vector<PaintCommand>& commands() const;
    void append(const DisplayList& other);
};

// Walk layout tree → display list
class Painter {
public:
    DisplayList paint(const layout::LayoutTree& tree);
};

// Stacking context management (z-index)
class StackingContext;

}
```

**Architecture:**

- Paint order follows CSS painting order (backgrounds → borders → block children → floats → inline children → outlines → positioned children), respecting stacking contexts
- Display list is an intermediate representation — compositor consumes it
- Display list items carry bounding rects for culling (skip off-screen items)
- Opacity and transforms are push/pop pairs for compositor layer boundaries

**Prompt template:**
> Implement paint module for Clever engine in C++20.
> Painter walks LayoutTree in CSS painting order (spec section 10.2 of CSS2):
> stacking contexts ordered by z-index, within each: background/border,
> block-level children (non-positioned, non-float), floats, inline content,
> positioned children. Output: DisplayList of PaintCommands (FillRect,
> DrawText, DrawImage, DrawBorder, DrawBoxShadow, PushClip/PopClip,
> PushOpacity/PopOpacity, PushTransform/PopTransform).
> Each command has bounding rect for compositor culling.
> StackingContext tree built from elements with z-index, opacity<1,
> transform, or position:fixed/sticky.
> Tests: paint order for overlapping positioned elements, opacity grouping,
> clipped overflow, stacking context ordering.

---

### 3.13 Compositor (`src/compositor/`)

**Phase 3 | ~20K LOC**

**Purpose:** Turn display lists into pixels on screen. Tile-based GPU rendering via Skia.

**Key types:**

```cpp
namespace clever::compositor {

struct Tile {
    int x, y;             // tile grid position
    int width, height;    // typically 256x256
    bool dirty = true;
    // GPU texture handle
};

class TileGrid {
public:
    void set_content_size(int width, int height);
    void mark_dirty(const Rect& area);
    std::vector<Tile*> dirty_tiles();
};

class Compositor {
public:
    // Rasterize dirty tiles from display list
    void rasterize(const paint::DisplayList& display_list, TileGrid& tiles);

    // Composite tiles to screen (GPU)
    void composite(const TileGrid& tiles, const Viewport& viewport);

    // Scroll without re-rasterizing (translate tile positions)
    void scroll(float dx, float dy);
};

class SkiaBackend {
public:
    void draw_rect(const Rect& rect, const Paint& paint);
    void draw_text(const text::ShapedRun& run, float x, float y);
    void draw_image(const image::DecodedImage& image, const Rect& dest);
    // etc.
};

}
```

**Architecture:**

- **Tiled rendering:** content divided into 256x256 tiles. Only dirty tiles are re-rasterized.
- **Skia:** All actual pixel rendering goes through Skia (GPU-accelerated via Metal on macOS, Vulkan on Linux)
- **Compositor runs on GPU thread** — receives display lists from renderer, rasterizes on worker threads, composites on GPU thread
- **Scrolling:** translate tile positions without re-rasterizing (until new tiles are needed)
- **Layer promotion:** elements with `will-change`, `transform`, `opacity <1`, `position: fixed` get their own compositor layer

**Deps:** Skia

**Prompt template:**
> Implement compositor for Clever engine in C++20.
> Tile-based rendering: TileGrid divides content into 256x256 tiles.
> Compositor rasterizes dirty tiles using Skia (SkCanvas).
> SkiaBackend implements all paint commands (FillRect → SkCanvas::drawRect,
> DrawText → SkCanvas::drawTextBlob from ShapedRun glyphs, DrawImage →
> SkCanvas::drawImage, borders, shadows, clipping, transforms, opacity).
> Compositing: combine rasterized tiles into final frame, submit to GPU
> (Metal on macOS, Vulkan on Linux). Scroll optimization: translate tile
> offsets, only rasterize newly exposed tiles.
> Tests: rasterize simple display list, verify tile dirtying, scroll offset.

---

### 3.14 JavaScript Integration (`src/js/`)

**Phase 3 | ~20K LOC**

**Purpose:** Embed V8, provide bridge between JS execution and the DOM.

**Spec scope:**

- `[ECMA]` ES2020+ (provided by V8)
- Script loading and execution (`<script>`, `<script defer>`, `<script async>`)
- Module scripts (`<script type="module">`)
- Event loop integration (microtasks, macrotasks, requestAnimationFrame)
- `setTimeout`, `setInterval`, `queueMicrotask`

**Key types:**

```cpp
namespace clever::js {

class Isolate {
public:
    static Isolate& current();
    void initialize();
    void shutdown();
    v8::Isolate* v8_isolate();
};

class ScriptContext {
public:
    explicit ScriptContext(dom::Document& document);

    // Execute script in this context
    ScriptResult execute(std::string_view source, std::string_view url);

    // Event loop
    void drain_microtasks();
    void run_timers();
    uint32_t set_timeout(v8::Local<v8::Function> callback, int delay_ms);
    uint32_t set_interval(v8::Local<v8::Function> callback, int interval_ms);
    void clear_timer(uint32_t id);

    // requestAnimationFrame
    uint32_t request_animation_frame(v8::Local<v8::Function> callback);
    void run_animation_frames(double timestamp);
};

}
```

**Architecture:**

- One V8 Isolate per renderer process
- One V8 Context per Document (iframe gets its own Context)
- Script execution is synchronous within the renderer's main thread
- Event loop: microtask checkpoint after each script/callback, then macrotask queue
- `requestAnimationFrame` callbacks run before each paint
- Script loading: parser-blocking `<script>` pauses tree construction, `defer` runs after parse, `async` runs when loaded

**Pipeline integration:**

```
Parse HTML → encounter <script> → pause parser
  → fetch script (if src) → execute script
  → script may modify DOM → resume parser
```

**Deps:** V8

**Prompt template:**
> Implement JavaScript integration for Clever engine in C++20.
> Embed V8. One Isolate per process, one Context per Document.
> ScriptContext ties a V8 Context to a dom::Document.
> Script loading: handle <script> (parser-blocking), <script defer>,
> <script async>, <script type="module">.
> Event loop: microtask queue (drain after each task), macrotask queue
> (setTimeout/setInterval), requestAnimationFrame queue (run before paint).
> Bridge: window object, document object — actual DOM bindings are in
> the bindings module, this module provides the scaffolding.
> Tests: execute simple script, setTimeout fires, microtask ordering,
> script modifying DOM via bridge.

---

### 3.15 Web API Bindings (`src/bindings/`)

**Phase 3 | ~40K LOC (mostly generated)**

**Purpose:** Expose DOM and Web APIs to JavaScript. Generated from WebIDL.

**Spec scope — "90% web" APIs:**

```
DOM:              Node, Element, Document, Text, Event, EventTarget,
                  HTMLElement subclasses, classList, dataset, style
CSSOM:            CSSStyleDeclaration, getComputedStyle, window.matchMedia
Fetch:            fetch(), Request, Response, Headers
XHR:              XMLHttpRequest (basic — many sites still use it)
URL:              URL, URLSearchParams
Storage:          localStorage, sessionStorage
Timers:           setTimeout, setInterval, clearTimeout, clearInterval
Console:          console.log/warn/error/info
Navigator:        navigator.userAgent, navigator.language
Location:         window.location (get/set/reload)
History:          window.history (pushState, replaceState, back, forward)
Encoding:         TextEncoder, TextDecoder
Canvas 2D:        CanvasRenderingContext2D (basic drawing — Phase 5)
JSON:             JSON.parse, JSON.stringify (V8 built-in)
Promise:          Promise (V8 built-in)
FormData:         FormData
Blob/File:        Blob, File, FileReader (basic)
IntersectionObserver: IntersectionObserver (Phase 5)
MutationObserver: MutationObserver (basic)
```

**Architecture:**

- **WebIDL code generator** (tool written in Python or C++) reads `.webidl` files and generates C++ V8 binding code
- Generated code handles: type conversion (JS ↔ C++), exception handling, overload resolution, optional/default parameters, callback wrapping
- Each Web API interface gets: a V8 wrapper class, a C++ implementation class, a template installation function
- Wrapper classes use V8's `ObjectTemplate` with accessors and method callbacks

**Prompt template:**
> Implement Web API binding infrastructure for Clever engine.
> Phase 1: Build WebIDL code generator (Python script) that reads .webidl
> files and outputs C++ V8 binding code. Generator handles: interface
> inheritance, attributes (getters/setters), methods, constructors, callback
> functions, enums, dictionaries, nullable types, sequence types.
> Phase 2: Write .webidl files for core APIs (Node, Element, Document,
> Event, EventTarget, HTMLElement, CSSStyleDeclaration, fetch, XHR,
> localStorage, URL, console, setTimeout). Generate bindings.
> Phase 3: Implement backing C++ classes that the bindings delegate to,
> connecting to dom::, css::, net:: modules.
> Tests: call DOM methods from JS, event handling from JS, fetch() from JS.

---

### 3.16 Storage (`src/storage/`)

**Phase 4 | ~8K LOC**

**Purpose:** localStorage, sessionStorage, cookies-on-disk, cache storage.

**Spec scope:**

- Web Storage API (localStorage, sessionStorage)
- IndexedDB — **defer to Phase 5**
- Cache API — **defer to Phase 5**
- Cookie persistence (cookies already modeled in net module, this adds disk persistence)

**Key types:**

```cpp
namespace clever::storage {

class StorageArea {
public:
    std::optional<std::string> get_item(const std::string& key) const;
    void set_item(const std::string& key, const std::string& value);
    void remove_item(const std::string& key);
    void clear();
    std::size_t length() const;
    std::string key(std::size_t index) const;
};

class StorageManager {
public:
    StorageArea& local_storage(const url::URL& origin);
    StorageArea& session_storage(const url::URL& origin);
};

}
```

**Deps:** SQLite (for localStorage persistence)

**Prompt template:**
> Implement Web Storage for Clever engine in C++20.
> StorageArea: key-value string store with get/set/remove/clear/length/key().
> localStorage: persisted to SQLite, partitioned by origin.
> sessionStorage: in-memory, partitioned by origin, cleared on process exit.
> StorageManager: provides StorageArea instances per origin.
> 5MB quota per origin for localStorage.
> Tests: set/get/remove, quota enforcement, origin isolation,
> persistence across StorageManager instances (for localStorage).

---

### 3.17 DevTools (`src/devtools/`)

**Phase 5 | ~15K LOC**

**Purpose:** Chrome DevTools Protocol (CDP) server for debugging.

**Defer to Phase 5.** Initial implementation: DOM inspector, console, network waterfall.

---

### 3.18 UI Shell (`src/shell/`)

**Phase 4 | ~10K LOC**

**Purpose:** Browser chrome — address bar, tabs, back/forward, bookmarks.

**Architecture:**

- Minimal native UI using platform windowing (Cocoa on macOS, GTK on Linux)
- Renders the browser chrome natively (not using the engine itself — avoids chicken-and-egg)
- Manages renderer process lifecycle (spawn, kill, navigate)
- Tab management
- Address bar with URL input and display
- Back/forward/reload buttons
- Page loading indicator

**Prompt template:**
> Implement browser UI shell for Clever engine in C++20.
> macOS target: use Cocoa (NSWindow, NSView, NSTextField for address bar).
> Minimal chrome: tab bar (create/close/switch), address bar (URL input +
> display, intercepts Enter to navigate), back/forward/reload buttons,
> loading indicator. Content area hosts renderer output (compositor
> presents into an NSView/Metal layer).
> Process management: spawn renderer process per tab, send navigation
> messages via IPC, receive compositor output.
> Tests: mostly manual/integration — but unit test tab state management.

---

### 3.19 Build System and Infrastructure

**Phase 1 | ~3K LOC**

**Build:** CMake 3.20+ with vcpkg for dependencies.

**Dependencies managed via vcpkg:**

```
icu, harfbuzz, freetype, libpng, libjpeg-turbo, libwebp, giflib,
skia, openssl (or boringssl), nghttp2, c-ares, sqlite3
```

V8 is special — either build from source (via depot_tools) or use prebuilt binaries.

**Directory structure:**

```
clever/
├── CMakeLists.txt
├── cmake/                    # CMake modules, find scripts
├── vcpkg.json                # Dependency manifest
├── src/
│   ├── platform/
│   ├── ipc/
│   ├── url/
│   ├── net/
│   ├── html/
│   ├── dom/
│   ├── css/
│   │   ├── parser/
│   │   └── style/
│   ├── layout/
│   ├── text/
│   ├── image/
│   ├── paint/
│   ├── compositor/
│   ├── js/
│   ├── bindings/
│   ├── storage/
│   ├── devtools/
│   └── shell/
├── include/
│   └── clever/               # Public headers mirror src/ structure
├── tests/
│   ├── unit/                 # Per-module unit tests
│   ├── integration/          # Cross-module tests
│   ├── wpt/                  # Web Platform Tests runner
│   └── fixtures/             # Test HTML/CSS/JS files
├── tools/
│   ├── webidl_codegen/       # WebIDL → C++ binding generator
│   └── test_runner/          # WPT test harness
└── docs/                     # Architecture docs only (no marketing)
```

**Test infrastructure:**

- Unit tests: GoogleTest
- Integration tests: Load HTML fixtures, verify layout geometry / rendered output
- WPT: Web Platform Tests — the standard conformance suite. Run subset relevant to implemented features.
- CI: Build + unit tests + WPT subset on every commit

**Prompt template:**
> Set up CMake build system for Clever engine.
> Top-level CMakeLists.txt with add_subdirectory for each module.
> vcpkg.json manifest listing all dependencies.
> Each module is a static library target.
> clever_engine library links all modules.
> clever_browser executable links clever_engine + shell.
> GoogleTest for unit tests, one test target per module.
> V8 integration: either find_package or ExternalProject_Add for
> building V8 from source. Skia: same approach.
> Compiler flags: -std=c++20, -Wall -Wextra -Werror, sanitizer builds
> (ASan, UBSan, TSan). LTO for release builds.

---

## 4. Implementation Phases

### Phase 1: Foundation (build system + platform + IPC + URL + networking)

Everything needed before you can load a page. At the end of Phase 1, you can:

- Fetch an HTTP(S) URL and receive the response body
- Parse URLs
- Spawn processes and communicate via IPC

**Modules:** 3.1, 3.2, 3.3, 3.4, 3.19

### Phase 2: Parsing and Style (HTML parser + DOM + CSS parser + style engine + text + images)

At the end of Phase 2, you can:

- Parse HTML into a DOM tree
- Parse CSS into style rules
- Resolve computed styles for every element
- Shape text into glyphs
- Decode images

**Modules:** 3.5, 3.6, 3.7, 3.8, 3.10, 3.11

### Phase 3: Rendering Pipeline (layout + paint + compositor + JS + bindings)

At the end of Phase 3, you can:

- Lay out the page
- Paint to a display list
- Composite to the screen via Skia
- Execute JavaScript that interacts with the DOM
- Load and render real web pages

**Modules:** 3.9, 3.12, 3.13, 3.14, 3.15

### Phase 4: Shell and Storage (browser chrome + persistence)

At the end of Phase 4, you have a usable browser:

- Tab management, address bar, navigation
- localStorage, cookies on disk

**Modules:** 3.16, 3.18

### Phase 5: Polish (DevTools, Canvas 2D, SVG, IndexedDB, accessibility)

Not needed for "90% web" but improves developer experience and site compatibility.

**Modules:** 3.17, plus extensions to existing modules

---

## 5. Data Flow: Loading a Page

```
1. User types URL in address bar
2. Shell sends NavigateRequest via IPC to browser process
3. Browser process: URL parse → HSTS check → DNS resolve → TCP connect → TLS handshake → HTTP request
4. Response headers arrive → check CSP, CORS → determine content type
5. HTML body streams to renderer process via IPC
6. Renderer:
   a. HTML Tokenizer consumes bytes → tokens
   b. Tree Builder consumes tokens → DOM nodes
   c. When <link rel="stylesheet"> encountered → request CSS via IPC → parse CSS
   d. When <script> encountered → request script via IPC → execute in V8
      (script may modify DOM — go back to step b for inserted content)
   e. When <img> encountered → request image via IPC → decode async
   f. After parse completes (or at first paint heuristic):
      - Style resolution: match selectors, cascade, compute styles (parallel)
      - Layout: compute box geometry (parallel per BFC)
      - Paint: walk layout tree → display list
      - Compositor: rasterize dirty tiles → composite to screen
7. User sees the page
8. JS event handlers run on user interaction → may mutate DOM → incremental
   style/layout/paint for dirty subtrees only
```

---

## 6. Memory and Safety

**C++ memory safety strategy** (Chromium found 70% of security bugs were memory safety issues):

1. **Ownership rules:**
   - DOM tree: parent owns children via `unique_ptr`. Parent pointer is raw (non-owning).
   - Style data: `ComputedStyle` owned by style tree, immutable once computed. Layout references via `const*`.
   - Layout tree: separate tree mirroring DOM, owns LayoutNodes via `unique_ptr`.
   - Display list: value type, copied to compositor.

2. **No raw `new/delete`:**
   - Use `std::make_unique`, `std::make_shared` exclusively.
   - Use `std::span` for non-owning views into arrays.
   - Use `std::string_view` for non-owning string references (careful about lifetime).

3. **Sanitizer builds:**
   - ASan (Address Sanitizer) in debug builds
   - UBSan (Undefined Behavior Sanitizer) in debug builds
   - TSan (Thread Sanitizer) for parallel style/layout code

4. **Fuzzing:**
   - HTML parser: fuzz with libFuzzer (same approach as Chromium/Ladybird)
   - CSS parser: fuzz with libFuzzer
   - Image decoders: fuzz with libFuzzer
   - URL parser: fuzz with libFuzzer

---

## 7. Key Chromium Mistakes to Avoid

| Mistake | What Chromium did | What Clever does |
|---------|-------------------|------------------|
| Mutable shared pipeline data | DOM/style/layout all mutate shared structures | Immutable snapshots between stages |
| Interleaved pipeline stages | JS can run mid-layout triggering forced sync layout | Pipeline stages complete fully before next begins; JS yields at defined points |
| Main thread contention | JS and rendering compete for main thread | Compositor on separate thread; style/layout parallelize on worker pool |
| C++ memory unsafety | 70% of security bugs | unique_ptr ownership, sanitizer CI, fuzzing |
| Unbounded process count | Process per site = huge memory | Process per tab with consolidation for same-site iframes |
| Complex embedder API | Content API surface is enormous | Clean module boundaries with generated bindings |

---

## 8. Testing Strategy

| Level | What | How |
|-------|------|-----|
| Unit | Individual functions and classes | GoogleTest, one test file per source file |
| Module | Module API contracts | Contract tests (like vibrowser's ContractValidator) |
| Integration | Multi-module pipelines | Load HTML fixture → verify DOM/style/layout/paint output |
| WPT | Spec conformance | Run Web Platform Tests subset, track pass rate |
| Fuzz | Parser/decoder robustness | libFuzzer for HTML/CSS/URL/image parsers |
| Visual | Rendering correctness | Reference image comparison (screenshot test) |
| Performance | No regressions | Benchmark suite for style/layout/paint on standard pages |

**WPT target:** 50% in 6 months, 70% in 12 months, 90%+ eventually.

---

## 9. Dependency Versions (as of 2026)

| Dep | Version | Purpose |
|-----|---------|---------|
| V8 | 13.x+ | JavaScript engine |
| Skia | m130+ | 2D graphics |
| ICU | 74+ | Unicode, BiDi, line breaking, IDNA |
| HarfBuzz | 8.x+ | Text shaping |
| FreeType | 2.13+ | Font rasterization |
| BoringSSL | latest | TLS |
| nghttp2 | 1.60+ | HTTP/2 framing |
| c-ares | 1.28+ | Async DNS |
| libpng | 1.6+ | PNG decoding |
| libjpeg-turbo | 3.0+ | JPEG decoding |
| libwebp | 1.3+ | WebP decoding |
| SQLite | 3.45+ | Storage |
| GoogleTest | 1.14+ | Testing |

---

## 10. Quick Reference: Spawning Implementation Work

Each module section (3.1–3.19) contains a **prompt template**. To implement a module:

1. Create a new working branch: `clever/module-name`
2. Use the prompt template as the starting instruction
3. Add context: "You are working on the Clever browser engine. Read CLEVER_ENGINE_BLUEPRINT.md section 3.X for full context. The module should follow the types and architecture described there."
4. Implement with TDD: write tests first, then implementation
5. Run tests, iterate until green
6. Integration test: verify the module works with its dependencies

For modules that depend on others not yet built, stub the dependency interfaces and implement against stubs. Replace stubs when upstream modules are ready.

---

## 11. Definition of Done

A module is done when:

- [ ] All types from the spec section are implemented
- [ ] Unit tests cover public API surface
- [ ] Integration tests verify cross-module behavior
- [ ] ASan/UBSan clean
- [ ] Code compiles with `-Wall -Wextra -Werror`
- [ ] No raw `new`/`delete` (only smart pointers)
- [ ] Public API is documented with brief comments (what, not how)

The engine is "v0.1" when:

- [ ] Can load and render `https://example.com` correctly
- [ ] Can load and render a basic page with CSS, images, and JavaScript
- [ ] Passes >20% of WPT tests for implemented features

The engine is "v1.0" when:

- [ ] Can render top-100 websites recognizably
- [ ] Passes >70% WPT for implemented features
- [ ] Multi-process architecture operational
- [ ] Scrolling and basic interaction works
