# Full Browser Backlog (Dependency-driven)

This backlog turns `docs/browser_engine_mvp_roadmap.md` into execution tickets and is intentionally stricter: every ticket has explicit dependencies and clear acceptance gates.

## Global Scope and Constraint
- Target goal: **full web standards support**, **high performance**, **high security**.
- Practical interpretation: build a browser shell and orchestration that can evolve to standards-complete behavior. Achieving complete parity directly from this codebase is a multi-year effort.
- Preferred execution path for “FULL”: use external upstream engines for standards-heavy components when pragmatic (e.g., parser/DOM/CSS/JS/render milestones), then integrate with this shell where possible.

## Ticket conventions
- **Priority**: `P0` (must block), `P1` (critical), `P2` (important), `P3` (nice/scale)
- **Track**:
  - `A` Architecture/Engine
  - `B` GUI/Shell
  - `C` Networking
  - `D` DOM/HTML
  - `E` CSS
  - `F` Layout
  - `G` Rendering
  - `H` JS runtime
  - `I` Events/Interaction
  - `J` Forms/Media/Input
  - `K` Security
  - `L` Standards Coverage/Conformance
  - `M` Testing/Infra
  - `N` Optimization
- IDs: `<Track><###>` e.g., `A-001`.

## Milestones
1. **Milestone 1 (M1): Browser Shell Foundation**
   - Architecture + GUI + basic async navigation + stable rendering pipeline.
2. **Milestone 2 (M2): Interactive Core**
   - DOM mutability + JS bridge + events + forms + scrolling.
3. **Milestone 3 (M3): Standards Expansion**
   - Strong networking, CSS/HTML spec growth, JS compatibility baseline, media.
4. **Milestone 4 (M4): Security + Hardening + Optimization**
   - Same-origin, CSP/CORP baseline, CSP, sandboxing, memory safety, perf.
5. **Milestone 5 (M5): Full Standards Track**
   - Conformance, automation, and long-term parity roadmap to full web platform features.

---

## 1) Core Architecture and Orchestration

### A-001 [P0] Create BrowserEngine facade
- **Description**: Introduce `browser::engine::Engine` that owns tabs, navigation state, cache handles, and lifecycle. Replace monolithic `run()`-style flow.
- **Files**: `include/browser/engine/engine.h`, `src/engine/engine.cpp`, `src/browser/browser.cpp`
- **Dependencies**: None
- **Acceptance**: `run(url)` becomes call into `Engine::open(url)`. Existing CLI entry still works.
- **Tests**: unit test navigation command dispatch and page state transitions.

### A-002 [P0] Document/Navigation state model
- **Description**: Add `PageState`, `NavigationState`, `LoadState`, and explicit lifecycle events.
- **Files**: `include/browser/engine/state.h`, `src/engine/state.cpp`
- **Dependencies**: A-001
- **Acceptance**: Can represent `loading -> parsed -> styled -> laid out -> painted -> done` states.
- **Tests**: state transition unit tests.

### A-003 [P0] Frame/task scheduler
- **Description**: Add a single-threaded task runner with priority queue (`micro`, `normal`, `layout`, `paint`, `network`).
- **Files**: `include/browser/scheduler/task.h`, `src/scheduler/task.cpp`
- **Dependencies**: A-002
- **Acceptance**: scheduled tasks are ordered and observable in trace logs.

### A-004 [P1] Render pipeline contracts
- **Description**: Define interface between layout tree, paint tree, and paint backends (surface, screenshot, debug).
- **Files**: `include/browser/render/pipeline.h`, `src/render/pipeline.cpp`
- **Dependencies**: A-001, A-003
- **Acceptance**: GUI and file output use same pipeline output structure.

### A-005 [P1] Error surfaces and diagnostics bus
- **Description**: Standardize warning/error message format with severity + source location.
- **Files**: `include/browser/error/diag.h`, `src/error/diag.cpp`
- **Dependencies**: A-001
- **Acceptance**: all loading/render steps attach user-visible diagnostic entries.

### A-006 [P2] Tab abstraction and session persistence
- **Description**: support multiple `PageState`s and per-tab back/forward/history serialization.
- **Files**: `include/browser/tab/tab.h`, `src/tab/tab.cpp`
- **Dependencies**: A-002
- **Acceptance**: create/close/switch tabs with no cross-tab memory bleed.

---

## 2) GUI Shell and Platform Integration

### B-001 [P0] Window runtime bootstrap
- **Description**: Integrate SDL2/GLFW backend behind `Window` abstraction; create main event loop.
- **Files**: `include/browser/ui/window.h`, `src/ui/window_sdl.cpp` (or glfw alternative)
- **Dependencies**: A-004
- **Acceptance**: opens window, close-to-exit cleanly, supports resize and repaint callback.

### B-002 [P0] Rendering surface adapter
- **Description**: Add texture/bitmap upload path from `Canvas`/paint ops to GUI surface.
- **Files**: `include/browser/ui/surface.h`, `src/ui/surface_sdl.cpp`
- **Dependencies**: B-001, A-004
- **Acceptance**: one frame rendered in GUI at fixed cadence.

### B-003 [P1] Address bar and chrome controls
- **Description**: Basic chrome UI controls (URL input, go/reload/back/forward).
- **Files**: `include/browser/ui/chrome.h`, `src/ui/chrome.cpp`
- **Dependencies**: B-001, A-001
- **Acceptance**: user can navigate by URL and see loading state text.

### B-004 [P1] Event input bridge (mouse/keyboard/touch)
- **Description**: Map platform events into browser `InputEvent` objects.
- **Files**: `include/browser/events/input.h`, `src/events/input.cpp`
- **Dependencies**: B-001, I-001
- **Acceptance**: click/move/scroll/key events delivered to engine.

### B-005 [P2] Developer console panel (basic)
- **Description**: In-window overlay with warnings/errors and JS logs.
- **Files**: `include/browser/ui/devtools_panel.h`, `src/ui/devtools_panel.cpp`
- **Dependencies**: B-001, A-005
- **Acceptance**: runtime warnings and script errors visible without log-file inspection.

---

## 3) Networking, Caching, and Resource Loading

### C-001 [P0] Async fetch scheduler
- **Description**: Implement non-blocking fetch jobs with cancellation and timeout.
- **Files**: `include/browser/net/fetch_scheduler.h`, `src/net/fetch_scheduler.cpp`
- **Dependencies**: A-003, C-002
- **Acceptance**: main thread never blocks on single resource fetch.

### C-002 [P0] Request descriptor + response object model
- **Description**: Introduce typed `Request`/`Response` and per-request metadata (headers, timings, cache headers).
- **Files**: `include/browser/net/request.h`, `include/browser/net/response.h`
- **Dependencies**: src/net/http_client.cpp
- **Acceptance**: all loads return `RequestId` + completion status.

### C-003 [P1] Redirect policy + URL hardening
- **Description**: Limit redirect depth, enforce scheme allow-list, preserve history correctly.
- **Files**: `src/net/url.cpp`, `src/net/http_client.cpp`
- **Dependencies**: C-002
- **Acceptance**: no open redirect loops; mixed content flagged.

### C-004 [P1] Resource cache layer (memory + disk)
- **Description**: Add LRU cache keyed by normalized URL + credentials mode.
- **Files**: `include/browser/net/cache.h`, `src/net/cache.cpp`
- **Dependencies**: C-001, C-002
- **Acceptance**: repeated loads hit cache and emit cache-miss/hit metrics.

### C-005 [P1] MIME sniffing & content-type policy
- **Description**: enforce expected MIME for CSS/JS/image/doc loads.
- **Files**: `src/net/http_client.cpp`, `src/browser/browser.cpp`
- **Dependencies**: C-002, D-001
- **Acceptance**: mismatched resources report warnings and follow deterministic fallback.

### C-006 [P2] HTTP cache-control + validators
- **Description**: implement `ETag`, `If-Modified-Since`, `Cache-Control` basics.
- **Files**: `src/net/http_client.cpp`, `src/net/cache.cpp`
- **Dependencies**: C-004
- **Acceptance**: stale resources are revalidated when applicable.

### C-007 [P2] Subresource preloader
- **Description**: parse HTML/CSS for high-priority resources and start early fetch.
- **Files**: `src/browser/browser.cpp`, `src/net/fetch_scheduler.cpp`
- **Dependencies**: C-001, D-002
- **Acceptance**: first paint improves for test fixture with CSS+script+image.

---

## 4) DOM and HTML Engine

### D-001 [P0] Formalize mutable DOM node API
- **Description**: add insertion/removal/reparent APIs; stable child/sibling maintenance.
- **Files**: `include/browser/html/dom.h`, `src/html/dom.cpp`
- **Dependencies**: A-004
- **Acceptance**: `appendChild/removeChild` operations do not corrupt tree.

### D-002 [P0] HTML parser incremental/error recovery mode
- **Description**: add deterministic recovery for malformed markup and script/style raw text safety.
- **Files**: `src/html/html_parser.cpp`, `src/html/html_parser.h`
- **Dependencies**: D-001
- **Acceptance**: malformed tags still generate usable DOM with warnings.

### D-003 [P1] DOM selection API completion
- **Description**: add query APIs for sibling traversal, `querySelector(All)`, nth filters, class/id/token selectors integration.
- **Files**: `include/browser/html/html_parser.h`, `src/html/html_parser.cpp`
- **Dependencies**: D-001
- **Acceptance**: JS calls and CSS selector engine both use same selector engine utilities.

### D-004 [P1] Node serialization and DOM introspection
- **Description**: provide safe snapshot serialization and inner/outer text APIs.
- **Files**: `src/html/dom.cpp`, `src/html/html_parser.cpp`
- **Dependencies**: D-001, A-005
- **Acceptance**: developer console can inspect node tree and attributes.

### D-005 [P2] Mutation events and observers
- **Description**: foundation for mutation observer and layout invalidation triggers.
- **Files**: `include/browser/html/mutation.h`, `src/html/mutation.cpp`
- **Dependencies**: D-001, I-002
- **Acceptance**: DOM changes emit mutation records and style/layout invalidation.

---

## 5) CSS and Style Engine

### E-001 [P0] Style tree and computed-style cache
- **Description**: build and cache computed style map per node with inheritance and origin precedence.
- **Files**: `include/browser/css/style_tree.h`, `src/css/style_tree.cpp`
- **Dependencies**: D-001, A-004
- **Acceptance**: style recompute path avoids full recomputation for unchanged subtrees.

### E-002 [P0] Style parser/validator expansion
- **Description**: add parser coverage for percentage units, common shorthands, `calc` basics, color and font keywords.
- **Files**: `src/css/css_parser.cpp`, `include/browser/css/css_parser.h`
- **Dependencies**: E-001
- **Acceptance**: fixture coverage for new value grammar set passes.

### E-003 [P1] Selector engine optimization
- **Description**: index-based selector matching, simple bloom/chain cache for right-to-left matching.
- **Files**: `src/css/css_parser.cpp`, `src/css/css_matcher.cpp` (new)
- **Dependencies**: E-001, D-003
- **Acceptance**: O(n*m) reduced on >1k node documents.

### E-004 [P1] Pseudo-classes/events linkage
- **Description**: support `:hover`, `:active`, `:focus`, `:visited` through state invalidation.
- **Files**: `src/css/css_parser.cpp`, `src/events/input.cpp`, `src/css/css_matcher.cpp`
- **Dependencies**: E-001, I-001, I-002
- **Acceptance**: visual state changes without page reload.

### E-005 [P2] @media and media query evaluator
- **Description**: minimal evaluator for screen/media width/height/orientation.
- **Files**: `src/css/css_media.cpp`, `src/css/css_parser.cpp`
- **Dependencies**: E-001
- **Acceptance**: style changes with resize in response to media queries.

### E-006 [P2] Cascade/composition improvements
- **Description**: implement cascade order by source, specificity, importance, inline overrides.
- **Files**: `src/css/css_parser.cpp`, `src/css/style_tree.cpp`
- **Dependencies**: E-001, E-002
- **Acceptance**: conflict tests match expected CSS cascade order.

---

## 6) Layout and Geometry

### F-001 [P0] Layout mode flags and computed geometry model
- **Description**: introduce explicit `DisplayType` state (`block`, `inline`, `inline-block`, `none`) and stable unit conversion.
- **Files**: `src/layout/layout_engine.cpp`, `include/browser/layout/layout_engine.h`
- **Dependencies**: E-001
- **Acceptance**: known layout fixtures match deterministic geometry results.

### F-002 [P0] Reflow entry points for DOM/style changes
- **Description**: implement incremental relayout on subtree dirty marks.
- **Files**: `src/layout/layout_engine.cpp`, `src/engine/engine.cpp`
- **Dependencies**: D-005, A-003
- **Acceptance**: style mutation does not require full-document rebuild unless root CSS changes.

### F-003 [P1] Overflow and clipping + viewport/scroller model
- **Description**: implement viewport model with scroll offsets and clipping regions.
- **Files**: `src/layout/layout_engine.cpp`, `include/browser/layout/layout_types.h`
- **Dependencies**: F-001
- **Acceptance**: scrollbars and clipped content work for overflowed containers.

### F-004 [P1] Relative/absolute/fixed positioning
- **Description**: implement three positioning modes and containing block behavior.
- **Files**: `src/layout/layout_engine.cpp`
- **Dependencies**: F-001
- **Acceptance**: positioned elements appear in expected coordinate space.

### F-005 [P2] Text measurement and line box engine upgrades
- **Description**: robust glyph metrics, wrapping, ellipsis, and baseline alignment.
- **Files**: `src/layout/layout_engine.cpp`, `src/render/renderer.cpp`
- **Dependencies**: F-001, G-002
- **Acceptance**: multi-line, mixed indentation, and wrapping tests pass consistently.

---

## 7) Render and Paint Backends

### G-001 [P0] Scene graph + paint-op generation
- **Description**: separate paint command recording from backend emission.
- **Files**: `include/browser/render/scene.h`, `src/render/scene.cpp`
- **Dependencies**: A-004, F-001
- **Acceptance**: both GUI renderer and PPM writer render from same scene description.

### G-002 [P1] Dirty region and invalidation tracking
- **Description**: minimal dirty-rect tracking from layout changes and invalidations.
- **Files**: `src/render/renderer.cpp`, `src/layout/layout_engine.cpp`
- **Dependencies**: G-001, F-002
- **Acceptance**: no redraw of unchanged regions in benchmark traces.

### G-003 [P1] Text rasterization abstraction
- **Description**: abstract font/text renderer interface to allow future font backends.
- **Files**: `include/browser/render/text_render.h`, `src/render/text_render.cpp`
- **Dependencies**: G-001
- **Acceptance**: fallback monospace path still works; interface supports replacement.

### G-004 [P2] Color management and compositing groundwork
- **Description**: alpha and blending support in paint ops.
- **Files**: `src/render/renderer.cpp`, `src/render/renderer.h`
- **Dependencies**: G-001
- **Acceptance**: semi-transparent overlay render tests pass.

### G-005 [P2] GPU backend plug-in path (optional)
- **Description**: integrate optional OpenGL/Vulkan texture uploader behind paint sink.
- **Files**: `src/render/pipeline.cpp`, `src/ui/surface_sdl.cpp`
- **Dependencies**: B-002, G-001
- **Acceptance**: software and GPU output parity via same scene graph.

---

## 8) JavaScript and Scripting

### H-001 [P0] Replace ad-hoc runtime with embeddable JS engine (QuickJS)
- **Description**: integrate QuickJS, create runtime bootstrap, expose host callbacks.
- **Files**: `include/browser/js/context.h`, `src/js/context.cpp`, `src/js/quickjs_bindings.cpp` (new)
- **Dependencies**: A-001, D-003, A-003
- **Acceptance**: run existing parser-executed samples with richer JS features.

### H-002 [P0] DOM bridge layer
- **Description**: expose `window`, `document`, `Element`, `Node`, style APIs, and event dispatch entrypoints.
- **Files**: `src/js/bindings_dom.cpp`, `include/browser/js/bindings.h`
- **Dependencies**: H-001, D-001
- **Acceptance**: `document.getElementById`, `querySelector`, `setAttribute` work from JS.

### H-003 [P1] Event APIs in JS
- **Description**: `addEventListener/removeEventListener/dispatchEvent` and basic `Event` object.
- **Files**: `src/js/bindings_events.cpp`, `include/browser/events/event.h`
- **Dependencies**: H-002, I-001
- **Acceptance**: click and key handlers run and mutate page.

### H-004 [P1] Timers and async model
- **Description**: implement `setTimeout`, `clearTimeout`, `setInterval`, basic `Promise` placeholders or microtask queue integration.
- **Files**: `src/js/timers.cpp`, `include/browser/js/timers.h`
- **Dependencies**: H-001, A-003
- **Acceptance**: async callbacks run on task queue and trigger repaint.

### H-005 [P2] Module/script-loading semantics
- **Description**: support module scripts and top-level defer/async ordering.
- **Files**: `src/js/runtime.cpp`, `src/browser/browser.cpp`, `src/net/fetch_scheduler.cpp`
- **Dependencies**: H-001, H-002, C-001
- **Acceptance**: dependent script ordering matches fixture expectations.

### H-006 [P2] Security hardening in runtime boundary
- **Description**: isolate script-global objects from host internals; validate method args and object life cycle.
- **Files**: `src/js/context.cpp`, `src/js/bindings.cpp`
- **Dependencies**: H-001
- **Acceptance**: untrusted script cannot crash host and cannot read raw file system APIs.

---

## 9) Events and User Interaction

### I-001 [P0] Hit-testing and pointer routing
- **Description**: map coordinates to layout box and DOM node.
- **Files**: `include/browser/events/hit_test.h`, `src/events/hit_test.cpp`
- **Dependencies**: F-001, G-001
- **Acceptance**: clicking a node targets correct handler node every time.

### I-002 [P0] Event propagation model
- **Description**: implement capture/bubble flow with cancelation and `preventDefault`.
- **Files**: `src/events/event_dispatch.cpp`, `include/browser/events/event.h`
- **Dependencies**: I-001, H-003
- **Acceptance**: bubbling order and target resolution test passes.

### I-003 [P1] Focus management and keyboard interaction
- **Description**: focus ring, tab-order, key dispatch to focused control.
- **Files**: `src/events/focus.cpp`, `src/ui/chrome.cpp`
- **Dependencies**: I-002, J-001
- **Acceptance**: tab through focusable elements and keyboard events route correctly.

### I-004 [P1] Drag/selection groundwork
- **Description**: text selection ranges and click-drag selection model.
- **Files**: `src/events/selection.cpp`
- **Dependencies**: I-001, F-001
- **Acceptance**: basic text selection visible for static text nodes.

### I-005 [P2] Gesture/scroll momentum + wheel normalization
- **Description**: consistent wheel scaling and basic kinetic scrolling.
- **Files**: `src/events/scroll.cpp`, `src/ui/window_sdl.cpp`
- **Dependencies**: F-003, I-001
- **Acceptance**: smooth scroll on trackpad/mouse wheel for long content.

---

## 10) Forms, Input Controls, and Accessibility

### J-001 [P0] Core form controls
- **Description**: implement `input`, `textarea`, `button`, `select` parsing + state, default styles.
- **Files**: `src/html/html_parser.cpp`, `src/layout/layout_engine.cpp`, `src/render/renderer.cpp`, `src/events/input.cpp`
- **Dependencies**: I-001, F-001
- **Acceptance**: form controls can be focused, edited, and submitted.

### J-002 [P1] Form submission pipeline
- **Description**: submit GET/POST with URL encoding, disabled-state handling.
- **Files**: `include/browser/forms/form.h`, `src/forms/form.cpp`
- **Dependencies**: J-001, C-001, C-002
- **Acceptance**: simple login/search forms work against local test endpoints.

### J-003 [P2] Accessibility foundations
- **Description**: implement aria-label/title/tooltips propagation into accessibility tree.
- **Files**: `include/browser/accessibility/tree.h`, `src/accessibility/tree.cpp`
- **Dependencies**: D-001, D-004
- **Acceptance**: basic accessibility dump available for node inspection.

### J-004 [P2] Media tags basics
- **Description**: image decoding and rendering path for img/video placeholders.
- **Files**: `src/net/fetch_scheduler.cpp`, `src/render/renderer.cpp`, `src/media/media.cpp`
- **Dependencies**: C-007, G-001
- **Acceptance**: local and remote images render with intrinsic dimensions.

---

## 11) Security, Sandboxing, and Isolation

### K-001 [P0] URL and input sanitization layer
- **Description**: validate and canonicalize all URL-like attributes and block dangerous schemes.
- **Files**: `src/net/url.cpp`, `src/browser/browser.cpp`
- **Dependencies**: C-003, D-001
- **Acceptance**: `javascript:` and other blocked schemes never execute directly.

### K-002 [P1] Origin model and same-origin checks
- **Description**: assign origin to each document/resource; enforce same-origin in script/subresource-sensitive operations.
- **Files**: `include/browser/security/origin.h`, `src/security/origin.cpp`
- **Dependencies**: K-001, C-002, H-001
- **Acceptance**: cross-origin blocked paths produce clear console diagnostics.

### K-003 [P1] CSP baseline
- **Description**: parse and enforce a minimal policy for script, img, style sources.
- **Files**: `src/security/csp.cpp`, `include/browser/security/csp.h`
- **Dependencies**: K-002, C-005
- **Acceptance**: inline script policy and remote script allow-list tested.

### K-004 [P2] Cookie jar and credential isolation
- **Description**: add cookie jar per origin with same-site defaults.
- **Files**: `src/net/cookies.cpp`, `include/browser/net/cookies.h`
- **Dependencies**: K-002, C-002
- **Acceptance**: authenticated page scenario works with cookie send/receive semantics.

### K-005 [P2] Process boundary planning (future hardening)
- **Description**: define clean IPC contracts for browser/render/network isolation path.
- **Files**: `docs/process_boundaries.md`
- **Dependencies**: A-001, K-001
- **Acceptance**: design doc reviewed; migration path to multi-process identified.

---

## 12) Conformance and Standards-Complete Track

### L-001 [P2] Standards tracker and issue matrix
- **Description**: create a living mapping of W3C/WCAG/WHATWG features with implementation state (Not implemented/Partial/FULL).
- **Files**: `docs/standards_matrix.md`
- **Dependencies**: D-001
- **Acceptance**: every major area has implementation status and owner.

### L-002 [P2] Add WPT-derived targeted subsets
- **Description**: create focused, curated Web Platform Tests per area and maintain pass-fail expectation.
- **Files**: `tests/wpt_subset/*`
- **Dependencies**: L-001, M-001
- **Acceptance**: CI reports per-subset pass rate and trend.

### L-003 [P2] Web API scaffolding
- **Description**: add initial APIs for DOM events, history, timers, viewport, geometry, storage (local/session storage placeholder).
- **Files**: `include/browser/api/{dom,time,history,storage}.h`, `src/api/*.cpp`
- **Dependencies**: H-002, I-002
- **Acceptance**: API stubs exist with tested behavior where relevant.

### L-004 [P3] Incremental full-standards bridge strategy (WebKit/Blink/Gecko integration)
- **Description**: define adapter layer that can switch from in-house parsers to external engine for specific modules.
- **Files**: `docs/standards_bridge_strategy.md`, `src/integration/engine_bridge/`
- **Dependencies**: L-001, A-001
- **Acceptance**: architecture supports module-by-module external drop-in.

### L-005 [P3] Internationalization and Unicode correctness
- **Description**: add full Unicode text shaping path, bidirectional text basics, locale number/date APIs.
- **Files**: `src/layout/unicode.cpp`, `src/render/text.cpp`
- **Dependencies**: F-005, G-003
- **Acceptance**: multilingual fixtures render with correct glyph sequence.

---

## 13) Testing and Quality Infrastructure

### M-001 [P0] Unit test harness unification
- **Description**: create `ctest`/`Catch2`/`GTest` baseline with coverage for core modules.
- **Files**: `CMakeLists.txt`, `tests/unit/*`
- **Dependencies**: A-001
- **Acceptance**: `ctest` runs from clean build and executes baseline 100+ tests.

### M-002 [P1] Golden render / screenshot regression
- **Description**: add deterministic render outputs and image diff checks.
- **Files**: `tests/render_golden/*`, `scripts/screenshot_diff.py` (or C++ harness)
- **Dependencies**: G-001, M-001
- **Acceptance**: CI gate fails if diff exceeds threshold.

### M-003 [P1] Integration scenario tests
- **Description**: navigation, back-forward, reload, form submit, script interaction.
- **Files**: `tests/integration/*`
- **Dependencies**: A-006, B-003, J-001
- **Acceptance**: core user flows complete end-to-end in CI.

### M-004 [P2] Performance harness and budgets
- **Description**: micro and macro benchmarks for parse/layout/paint and memory behavior.
- **Files**: `tests/bench/*`
- **Dependencies**: N-001
- **Acceptance**: budgets and alerts defined for key scenarios.

### M-005 [P2] Fuzz + security tests
- **Description**: parser fuzz entry points and red-team-like invalid-input corpus.
- **Files**: `tests/fuzz/*`, `tests/security/*`
- **Dependencies**: D-002, C-001
- **Acceptance**: no crashes/hangs with fuzz corpus within budget.

---

## 14) Optimization and Security-by-Performance Track

### N-001 [P1] Profiling and tracing
- **Description**: add phase timers and frame traces to every major pipeline stage.
- **Files**: `include/browser/metrics/trace.h`, `src/metrics/trace.cpp`
- **Dependencies**: A-003, A-005
- **Acceptance**: every page load logs stage durations.

### N-002 [P1] Pooling and allocator pressure reduction
- **Description**: reduce repeated allocations for DOM nodes, style objects, and layout boxes.
- **Files**: `src/html/dom.cpp`, `src/css/css_parser.cpp`, `src/layout/layout_engine.cpp`
- **Dependencies**: F-002, N-001
- **Acceptance**: lower alloc count in scripted stress page by >30% vs baseline.

### N-003 [P2] Parallelizable parse phases
- **Description**: identify independent parse tasks (script/style subresources) for async parallel execution.
- **Files**: `src/browser/browser.cpp`, `src/net/fetch_scheduler.cpp`
- **Dependencies**: C-007, A-003
- **Acceptance**: large documents complete faster and remain responsive.

### N-004 [P2] Cache pressure and eviction policy
- **Description**: memory-aware caches with metrics and adaptive eviction.
- **Files**: `src/net/cache.cpp`, `src/layout/layout_engine.cpp`
- **Dependencies**: C-004, N-001
- **Acceptance**: memory plateau under long-running browsing sessions.

### N-005 [P3] Zero-copy fast paths
- **Description**: reduce copies in response body piping and paint path where safe.
- **Files**: `src/net/http_client.cpp`, `src/render/renderer.cpp`, `src/render/scene.cpp`
- **Dependencies**: N-001, G-002
- **Acceptance**: measurable throughput gain with no functional regressions.

---

## Dependency graph (condensed)

- `A-001 -> A-002, A-003, A-004, A-005`
- `A-004 -> B-001, B-002, G-001`
- `B-001 -> B-003, B-004`
- `C-001 -> C-004, C-007`
- `D-001 -> D-002, D-003, H-002, I-001`
- `E-001 -> F-001, F-002, E-002, E-004`
- `F-002 -> I-001, G-002`
- `H-001 -> H-002, H-003, H-004`
- `I-001 -> I-002 -> H-003`
- `J-001 -> J-002, J-003`
- `K-001 -> K-002 -> K-003`
- `M-001 -> M-002, M-003`

---

## Realistic note on “FULL web standards”

This backlog can drive to a **browser-tier compatibility target**, but full web standards parity is a multi-year, multi-engine effort. The practical high-confidence strategy is:

1. Keep the current modular architecture as a shell and orchestrator.
2. Preserve incremental deliverables from the MVP path.
3. For standards-critical surfaces (DOM/CSS/JS/rendering), adopt production-grade upstream libraries/engines where feasible.
4. Define strict compatibility budgets and acceptance gates per WPT subset.

If you want, I can now produce `docs/full_browser_gantt.md` from these tickets with exact sequencing by milestone, duration (story points), and a suggested 6-, 12-, 18-, 24-month plan. 
