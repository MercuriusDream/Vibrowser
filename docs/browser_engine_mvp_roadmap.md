# Browser Engine MVP Roadmap

This roadmap is written for the current codebase layout (`src/`, `include/`) and moves from your current static renderer to a browser-engine-like but narrow MVP.

Current implementation is a strong foundation for:
- URL normalization and fetch in `src/net`
- HTML + CSS parsing in `src/html`, `src/css`
- a synchronous render pipeline in `src/browser`
- basic JS mutation surface in `src/js`
- layout + raster output in `src/layout`, `src/render`

It is not currently a browser engine in the full interactive sense.

This document is intentionally strict: each part must be completed and verified by tests before moving to the next one.

## Definitions

1. **MVP target**: deterministic, interactive page engine with GUI, navigation, DOM, JS, events, forms, images, styling, and test suite.
2. **browser-engine-like**: architecture and behavior matching browser-era expectations for a single-process subset, not full Chrome multi-process hardening.
3. **Release-ready** means: automated builds, automated tests, and known compatibility envelope.

## Part 0 — Baseline and Scope Freeze

Goal: lock in what “browser-engine MVP” means so we do not overbuild prematurely.

### 0.1 Acceptance criteria
1. `README.md` contains a clear supported/unsupported matrix.
2. `docs/browser_engine_mvp_vision.md` is added with feature scope.
3. Build system emits explicit warnings when optional flags are missing (OpenSSL/GUI).

### 0.2 Concrete tasks
1. Add `docs/browser_engine_mvp_scope.md` with explicit pass/fail behavior list.
2. Define “must-have” and “later” buckets.
3. Decide hard dependencies:
   - Windowing: SDL2 or GLFW
   - JS runtime: QuickJS (first choice for embeddability)
   - Image decode: stb_image or equivalent (later)

### 0.3 Files to create/modify
1. `docs/browser_engine_mvp_scope.md`
2. `README.md`

## Part 1 — Core Runtime Architecture Refactor

Goal: make current pipeline explicit and replace one-shot command flow with a browser state model.

### 1.1 What to build
1. Introduce high-level app state types:
   - Browser session
   - Page/document controller
   - Navigation state
   - Resource and render cache
2. Replace direct CLI path (`run`) with an internal `BrowserEngine` orchestrator that can be called repeatedly (navigate, reload, resize, repaint).

### 1.2 Specific file-level work
1. Add:
   - `include/browser/engine/browser_engine.h`
   - `src/engine/browser_engine.cpp`
2. Introduce explicit pipeline interfaces:
   - `include/browser/engine/load_stage.h`
   - `include/browser/engine/layout_stage.h`
   - `include/browser/engine/paint_stage.h`
3. Keep `src/browser/browser.cpp` as adapter for CLI compatibility while forwarding to the engine.

### 1.3 State/data structures
1. Add
   - `browser::app::DocumentState` (URL, DOM root, stylesheet, layout, warnings)
   - `browser::app::NavigationRecord` (history id, title, URL, load result)
   - `browser::app::Viewport` (width, height, scale)

### 1.4 Acceptance criteria
1. `run(url, options)` can be represented as `engine.open(url)` + `engine.render_frame()`.
2. Existing CLI output flow remains working.
3. No behavior regressions from current CLI run path.

## Part 2 — GUI Surface (Mandatory for full-on browser UX)

Goal: move from PPM output to an interactive window and event loop.

### 2.1 Window layer
1. Add window module:
   - `include/browser/ui/window.h`
   - `src/ui/window_sdl.cpp` (or GLFW variant)
2. Responsibilities:
   - window creation
   - resize callbacks
   - keyboard/mouse event queue
   - pixel upload/render path to GPU buffer or software blit
3. Implement minimal chrome UI controls:
   - address bar
   - Go / Reload / Back / Forward buttons
   - optional status text bar

### 2.2 Paint surface integration
1. Replace PPM write path from render module for normal mode.
2. Add `RenderSurface` abstraction:
   - `include/browser/ui/render_surface.h`
   - `src/ui/render_surface_sdl.cpp`
3. Implement frame pump:
   - `engine.tick()` each loop iteration
   - dirty-flag-based repaint

### 2.3 Input dispatch contract
1. Translate SDL/GLFW events into internal event objects:
   - `MouseMove`, `MouseButtonDown`, `MouseButtonUp`, `Wheel`
   - `KeyDown`, `KeyUp`, `TextInput`
2. Event bus location:
   - `include/browser/events/event.h`
   - `src/events/event_queue.cpp`

### 2.4 Acceptance criteria
1. Window opens and displays current page.
2. Resizing redraws correctly.
3. Mouse movement + click routed to browser engine (even if currently no page behavior).

## Part 3 — Networking and Subresource Loading

Goal: robust navigation and fetch behavior closer to browser baseline.

### 3.1 Core fetch model
1. Extend `browser::net` with request descriptors:
   - method (`GET` minimum, `POST` later)
   - headers
   - timeout/retry settings
   - redirect policy
2. Add caching metadata keys (URL, ETag, Last-Modified, cache-control).

### 3.2 Async loading
1. Add a lightweight fetch scheduler to avoid blocking UI:
   - `include/browser/net/fetch_scheduler.h`
   - `src/net/fetch_scheduler.cpp`
2. Introduce task states: queued, in-flight, succeeded, failed, canceled.

### 3.3 Resource types
1. Document, stylesheet, script, image, font.
2. Add content-type checks and fallback:
   - HTML for main document
   - `text/css` for stylesheet
   - JS MIME for scripts
3. Add `resource://`? skip for MVP.

### 3.4 URL/base resolution improvements
1. Keep existing URL/canonicalization in `src/net/url.cpp`.
2. Add strict failure diagnostics for mixed-scheme redirects.
3. Improve relative URL resolution for query-only and hash-only navigation.

### 3.5 Acceptance criteria
1. `load(url)` never blocks UI thread.
2. Subresources from `link`, `style`, `script` load concurrently.
3. Redirects, errors, and timeouts produce visible status in UI.

## Part 4 — DOM and Document Semantics Expansion

Goal: establish robust, mutable DOM for JS + future CSS selector correctness.

### 4.1 DOM API upgrades
1. Extend `include/browser/html/dom.h` and `src/html/dom.cpp` with:
   - parent/sibling pointers cache
   - node removal + insertion helpers
   - `firstElementChild`, `children`, `childNodes` equivalents internally
2. Add mutation observers later; initially provide mutation side-effects for reflow triggers.

### 4.2 HTML parser hardening
1. Move from permissive tree recovery to tracked error nodes:
   - unclosed tags handled with consistent implied closures
   - script/style rawtext handling improvements
2. Add incremental parsing entry points for dynamic updates (string append / document rewrite).

### 4.3 Query API alignment
1. Expand selectors used by parser and CSS resolution:
   - `querySelector`, `querySelectorAll`
   - nth-child family already partly supported in CSS path
2. Expose these APIs through runtime bridge.

### 4.4 Acceptance criteria
1. Script/JS can modify DOM node structure and style.
2. Structural changes propagate to style/layout update path.
3. No memory leaks after repeated DOM rewrites in long sessions.

## Part 5 — CSS Engine Evolution

Goal: move from static parsing to a workable style-computation engine for interactive layout updates.

### 5.1 Parsing improvements
1. Add computed-style caching per node.
2. Handle `@media` better (at least `screen` + min-width/max-width).
3. Add support for more value parsing:
   - percentages in widths/margins
   - common font-size units
   - basic color keywords cache

### 5.2 Selector engine milestones
1. Keep current selector pipeline in `src/css/css_parser.cpp`.
2. Add:
   - `:hover`, `:active`, `:focus`, `:visited` (initially coarse)
   - selector matching by ancestry cache
3. Add property precedence and inheritance model:
   - explicit inherit behavior
   - initial/default values

### 5.3 Style invalidation
1. Add dependency graph from stylesheet + inline style + DOM attributes.
2. On mutation, recompute affected subtree styles only.

### 5.4 Acceptance criteria
1. Inline and external style updates from scripts are reflected visually.
2. Style recalculation has bounded cost and is logged.
3. Common selectors and box model basics pass fixture tests.

## Part 6 — Layout Engine Expansion

Goal: support practical page geometry and scrolling.

### 6.1 Layout model
1. Move toward box tree nodes with explicit computed boxes:
   - margin/border/padding
   - width/height constraints
   - position types (`static`, `relative`, `absolute`, maybe `fixed`)
2. Separate normal flow and positioned flow paths.

### 6.2 Block and inline treatment
1. Improve text measurement and word wrapping.
2. Add line-height inheritance and line box creation.
3. Add margin collapsing rules (limited first cut).

### 6.3 Scrolling and viewport
1. Add document/content height tracking.
2. Add scroll offsets and visible viewport mapping.

### 6.4 Acceptance criteria
1. Vertical overflow produces scrollable content.
2. Basic multi-line text and list-like structure render predictably.
3. Reflow after DOM updates is fast enough for medium pages.

## Part 7 — Rendering Pipeline (GPU-ready future)

Goal: keep software renderer but architect for swap to GPU later.

### 7.1 Render separation
1. Split scene generation and painting:
   - `Scene`/`PaintOps` generation from layout tree
   - back-end painter writes to either PPM file or GUI surface
2. Add dirty rectangles and partial repaint.

### 7.2 Painting features
1. Color, background, border, text rendering currently exist; add:
   - border radius initial
   - overflow clipping
   - alpha handling for future compositing
2. Preserve anti-aliasing path for font glyphs (still placeholder until font library integration).

### 7.3 Acceptance criteria
1. GUI mode and image export share one paint pipeline.
2. No tearing/flicker across resize and redraw cycles.

## Part 8 — JavaScript Runtime Upgrade (Largest blocker for MVP)

Goal: replace handcrafted mini runtime with embedded proper JS engine.

### 8.1 Runtime selection
1. Add QuickJS integration:
   - `third_party/quickjs` or package dependency
   - build integration in `CMakeLists.txt`
2. Define host bindings:
   - `window`, `document`, `Element`, `Event`, `console`

### 8.2 Execution lifecycle
1. Replace `src/js/runtime.cpp` with:
   - compiled script execution environment
   - error-to-string reporting
   - runtime stack traces
2. Provide execution contexts:
   - document parsing script execution
   - deferred async tasks

### 8.3 API surface for MVP
1. `document.getElementById`, `querySelector`, `querySelectorAll`
2. `element.innerText`, `innerHTML` (minimal), style property operations
3. DOM mutation APIs: `appendChild`, `removeChild`, `setAttribute`, `removeAttribute`
4. Event API: `addEventListener`, `dispatchEvent`
5. Timers: `setTimeout` and `setInterval` (single-threaded cooperative)

### 8.4 Security boundaries in engine
1. No access to host filesystem/network unless explicitly modeled.
2. Script execution errors must not crash rendering loop.

### 8.5 Acceptance criteria
1. Existing sample scripts execute and mutate DOM.
2. Script-caused DOM changes reflow/repaint deterministically.
3. Runtime errors appear in warning console without bringing process down.

## Part 9 — Event Model and User Interaction

Goal: support input-driven web behavior.

### 9.1 Core event dispatch
1. Add `Event` hierarchy:
   - mouse: `click`, `mousedown`, `mouseup`, `mousemove`, `mouseover`, `mouseout`
   - keyboard: `keydown`, `keyup`, `input`
   - form: `change`, `submit`
2. Add capture/bubble model (at least bubble).

### 9.2 Focus and active state
1. Maintain focus owner.
2. Update `:focus` class simulation and style invalidation.

### 9.3 Gesture/input path
1. Hit testing from coordinates to layout box and element.
2. Route coordinates into event target mapping.

### 9.4 Acceptance criteria
1. Clicking link/button fires handler hooks.
2. Keyboard input can modify focused text fields.
3. Event listeners can cancel default action.

## Part 10 — Forms, Media, and Graphics

Goal: support interactive HTML pages beyond static text.

### 10.1 Forms
1. Implement parsing and rendering for:
   - `input` types: `text`, `password`, `checkbox`, `radio`, `submit`
   - `textarea`, `button`, `select` (minimal)
2. Implement value APIs and submit event emission.

### 10.2 Images and media
1. Add image loader + decoder:
   - minimal PNG/JPG decoding
   - intrinsic size handling
2. Add `img` layout and paint path.

### 10.3 Links and navigation
1. `A` click navigates through engine navigation state.
2. `target` handling can be internal only for MVP.

### 10.4 Acceptance criteria
1. Common login/form demos work interactively.
2. External and local images display in layout.

## Part 11 — Security, Stability, and Process Contracts

Goal: move from toy behavior to browser-safe baseline.

### 11.1 Security basics
1. Content security policy-lite:
   - disable mixed content for HTTPS->HTTP by default
   - basic same-origin checks for script execution and XHR-like helpers
2. URL sanitization for resource fetch and `javascript:` blocks.

### 11.2 Stability
1. Hard memory caps for caches and task queues.
2. Timeout budgets for all async operations.

### 11.3 Crash containment
1. Wrap potentially failing engine steps with fail-safe boundaries.
2. Render broken pages with warning UI instead of aborting process.

### 11.4 Acceptance criteria
1. Malicious URL and invalid HTML do not crash engine.
2. Timeouts keep navigation responsive.

## Part 12 — Test and Benchmark Suite Infrastructure

Goal: prevent regressions and measure browser-like behavior.

### 12.1 Unit test expansion
1. Add directories:
   - `tests/unit/html`
   - `tests/unit/css`
   - `tests/unit/net`
   - `tests/unit/layout`
   - `tests/unit/js`
2. Add parser and style matching tests for edge-cases.

### 12.2 Integration tests
1. Add `tests/integration/navigation` with:
   - basic page load
   - stylesheet loading
   - script mutation + reload
2. Add screenshot comparison for GUI render output:
   - deterministic fixtures
   - baseline folder `tests/golden/`

### 12.3 End-to-end with sample pages
1. Local server fixture test for multiple resource loads.
2. Add navigation/history test.

### 12.4 Fuzz and stress
1. Random HTML/CSS fuzzer harness for parser safety.
2. large DOM and long-form text stress pages.

### 12.5 Acceptance criteria
1. CI runs at least 80% of suites on each commit.
2. Regression test added for each fixed browser-level defect.

## Part 13 — UX and Browser Shell MVP

Goal: user-facing shell to test the engine as browser, not renderer.

### 13.1 Minimal chrome
1. URL bar and go button.
2. Loading indicator and title/status line.
3. Error page for DNS/parse/network errors.

### 13.2 Bookmarks and session
1. Save/restores last URL and scroll state.
2. Local history list (`json` file in user cache dir).

### 13.3 Developer-facing diagnostics
1. Developer console output panel for warnings and script errors.
2. Optional DOM tree dump command.

### 13.4 Acceptance criteria
1. Browser can be used to browse a small list of static pages interactively.
2. Status/warnings visible and actionable.

## Part 14 — Engine Alignment Layer (Longer term but now guided)

Goal: start approximating engine component boundaries.

### 14.1 Layer mapping
1. UI Process = window + shell in `src/ui`
2. Browser Process = navigation, history, task scheduling in `src/engine`
3. Render/DOM process = DOM/CSS/Layout/paint in current modules
4. Network Process = async fetch module in `src/net`

### 14.2 Future work (not MVP)
1. Multi-process isolation
2. Audio/video stack
3. WebRTC/WebSocket/WebTransport
4. Extensions/DevTools Protocol

## Delivery Plan by Milestone

### Milestone A (Weeks 1-4)
1. Part 1: Runtime architecture
2. Part 2: GUI surface
3. Part 3: async load foundation
4. Part 12: unit test scaffolding

### Milestone B (Weeks 5-8)
1. Part 4: DOM expansion
2. Part 5: CSS improvements
3. Part 6: layout upgrades
4. Part 7: render separation

### Milestone C (Weeks 9-12)
1. Part 8: QuickJS runtime
2. Part 9: event model + interaction
3. Part 10: forms/images basics

### Milestone D (Weeks 13-16)
1. Part 11: stability/security
2. Part 12: integration + screenshot suites
3. Part 13: polished shell and completion polish

## Risk Register

1. Biggest risk is JS engine migration complexity.
2. Biggest performance risk is full-tree relayout from DOM changes.
3. Biggest compatibility risk is parser divergence from real browsers.
4. Biggest UX risk is fragile event-to-layout hit testing.

## Final Definition of “browser-engine-like MVP”

A run qualifies when all of these are true:
1. Load, navigate, reload, back/forward all work interactively.
2. Scripts can mutate DOM and attach event handlers.
3. Clicking and typing produce page-visible effects.
4. Styles load and recalc correctly for changed DOM.
5. Visual output is stable and scrollable in GUI window.
6. Navigation errors are visible and recoverable.
7. Tests cover parser/CSS/layout/script interaction.
8. Project builds reproducibly with documented dependencies.

