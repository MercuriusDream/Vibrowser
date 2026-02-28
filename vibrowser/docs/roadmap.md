# Vibrowser → Full Web Engine: Implementation Roadmap

> Generated from comprehensive 8-agent audit (2026-02-28)
> Covers: HTML parser, CSS resolver, layout engine, paint pipeline, JS engine, networking, DOM, platform

## Current Capabilities

| Subsystem | Status | Key Metrics |
|-----------|--------|-------------|
| HTML Parser | ~50% spec | 39/80 tokenizer states, 13/23 insertion modes, ~40/2231 entities |
| CSS Resolver | Strong | ~288 properties, cascade with @layer, CSS variables, calc() with trig |
| Layout Engine | Good | Block, inline, flex, basic grid, table, floats, positioning |
| Paint Pipeline | Good | Software renderer: gradients, transforms, filters, blend modes, clip-path |
| JS Engine | Moderate | QuickJS ES2020, 17,849 lines DOM bindings, fetch/XHR/WebSocket |
| Networking | Basic | HTTP/1.1 + TLS + gzip + chunked + caching (50MB LRU) |
| Platform | macOS | Native shell: tabs, history, bookmarks, zoom, find-in-page |
| Tests | 21,287 | 13 test suites, all passing |

## Phase 1 — "Make real sites load" (Tier 1 Quick Wins)

### 1. HTTP Keep-Alive + ConnectionPool
- **File**: `src/net/request.cpp` line 126
- **Problem**: `Connection: close` hardcoded. `ConnectionPool` class fully built but never used.
- **Fix**: Change header to `keep-alive`, wire up pool in `http_client.cpp`
- **Impact**: 3-5x faster page loads (reuse TCP+TLS connections)
- **Effort**: Days

### 2. Parallel Resource Fetching
- **Files**: `src/paint/render_pipeline.cpp`, `src/platform/thread_pool.cpp`
- **Problem**: ThreadPool exists but disconnected. All images/CSS/JS load serially.
- **Fix**: Queue resource fetches on ThreadPool, collect results before layout
- **Impact**: Pages with 20+ resources load in parallel
- **Effort**: Days

### 3. Text Decoration Rendering
- **Files**: `src/paint/painter.cpp`, `src/paint/display_list.cpp`
- **Problem**: `text_decoration` field exists in layout but is never rendered
- **Fix**: Read field in painter.cpp, draw underlines/overlines/strikethroughs
- **Impact**: All underlined links visible
- **Effort**: Hours

### 4. Text Shadow Rendering
- **Files**: `src/paint/painter.cpp`
- **Problem**: `text_shadow` fields exist but never consumed
- **Fix**: Apply shadow offset/blur/color during text painting
- **Impact**: Google.com headings, many modern sites
- **Effort**: Hours

### 5. Adoption Agency Algorithm
- **File**: `src/html/tree_builder.cpp`
- **Problem**: No handling of misnested tags (`<b><i></b></i>`)
- **Fix**: Implement spec's adoption agency algorithm with active formatting elements
- **Impact**: Dramatically better real-world HTML parsing
- **Effort**: 1 week

### 6. Complete HTML Entity Table
- **File**: `src/html/tokenizer.cpp`
- **Problem**: Only ~40 of 2,231 named entities implemented
- **Fix**: Generate full lookup table from WHATWG spec JSON
- **Impact**: Special characters render correctly (—, ©, €, etc.)
- **Effort**: Days

## Phase 2 — "Make real sites look right"

### 7. Stacking Contexts + z-index
- **Files**: `src/layout/layout_engine.cpp`, `src/paint/painter.cpp`
- **Problem**: z-index parsed and stored but never used in paint ordering
- **Fix**: Build stacking context tree, sort paint order by z-index
- **Impact**: Dropdowns, modals, popups, tooltips render correctly
- **Effort**: 1 week

### 8. Overflow + Scrolling
- **Files**: `src/layout/layout_engine.cpp`, `src/paint/painter.cpp`, `src/shell/render_view.mm`
- **Problem**: `overflow: scroll/auto/hidden` parsed but not implemented
- **Fix**: Clip overflow content, add scroll offset, handle scroll events
- **Impact**: Any page with scrollable content
- **Effort**: 1-2 weeks

### 9. CSS Grid Layout Algorithm
- **File**: `src/layout/layout_engine.cpp`
- **Problem**: Grid properties parsed as strings but no layout algorithm
- **Fix**: Track sizing, cell placement, auto-flow per CSS Grid spec
- **Impact**: Most modern sites use CSS Grid
- **Effort**: 2 weeks

### 10. Block Formatting Context + Margin Collapsing
- **File**: `src/layout/layout_engine.cpp`
- **Problem**: No BFC establishment, floats leak, incorrect margin collapse
- **Fix**: Track BFC boundaries, contain floats, implement parent-child margin collapse
- **Impact**: Correct float wrapping and spacing
- **Effort**: 1 week

## Phase 3 — "Make real sites interactive"

### 11. Event Default Actions
- **File**: `src/js/js_dom_bindings.cpp`
- **Problem**: 3-phase event bubbling works, but no default actions
- **Fix**: Add handlers: click on `<a>` → navigate, submit → form post, keypress → input
- **Impact**: Interactive websites actually work
- **Effort**: 1 week

### 12. Typed HTML Elements
- **File**: `src/js/js_dom_bindings.cpp`
- **Problem**: No HTMLInputElement.value, HTMLFormElement.submit(), etc.
- **Fix**: Create typed element proxies with spec properties
- **Impact**: Forms, inputs, any JS-driven interactivity
- **Effort**: 2 weeks

### 13. JS URL Constructor + TextEncoder/TextDecoder
- **File**: `src/js/js_window.cpp`
- **Problem**: Missing standard Web APIs
- **Fix**: Implement URL, URLSearchParams, TextEncoder, TextDecoder
- **Impact**: Unblocks many JS libraries
- **Effort**: Days

### 14. ES Modules
- **Files**: `src/js/js_engine.cpp`, `src/paint/render_pipeline.cpp`
- **Problem**: `<script type="module">` explicitly skipped (line 11991)
- **Fix**: Module loading, parsing, linking in QuickJS
- **Impact**: All modern JS frameworks require modules
- **Effort**: 2 weeks

## Phase 4 — "Make it fast"

### 15. Incremental/Async Rendering Pipeline
- **File**: `src/paint/render_pipeline.cpp` (12,345 lines!)
- **Problem**: Monolithic synchronous render_html() blocks main thread
- **Fix**: Break into stages, off-thread layout/paint, dirty rectangles
- **Impact**: UI stays responsive during page load
- **Effort**: 3 weeks

### 16. GPU Compositing
- **Files**: `src/paint/software_renderer.cpp`, `src/shell/render_view.mm`
- **Problem**: Pure software renderer (CGImage → NSView)
- **Fix**: Metal or Core Animation compositing layers
- **Impact**: 10-100x faster rendering, smooth scrolling
- **Effort**: 4 weeks

### 17. HTTP/2 Support
- **Files**: `src/net/http_client.cpp`, `src/net/tls_socket.cpp`
- **Problem**: Only HTTP/1.1
- **Fix**: HPACK, stream multiplexing, server push
- **Impact**: Required by many modern CDNs
- **Effort**: 2 weeks

### 18. CSS Animations & Transitions Runtime
- **Files**: `src/css/style/style_resolver.cpp`, `src/paint/render_pipeline.cpp`
- **Problem**: Parsed and stored but no runtime engine
- **Fix**: Keyframe interpolation, transition state machine, rAF
- **Impact**: Animated websites, hover effects
- **Effort**: 2 weeks

## Architecture Debt

| Issue | Current State | Fix |
|-------|--------------|-----|
| Dual DOM | C++ `dom::Node` hierarchy unused; `SimpleNode` is actual DOM | Unify on one DOM tree |
| Monolithic render_pipeline.cpp | 12,345 lines, one function | Break into parse/style/layout/paint stages |
| LayoutNode bloat | ~200+ fields, ~4-5KB per node | Split into style/geometry/paint structs |
| Synchronous main thread | Everything blocks UI | Async pipeline with main-thread dispatch |
| EventLoop/ThreadPool unused | Custom implementations exist but app uses GCD | Wire them up or remove |
