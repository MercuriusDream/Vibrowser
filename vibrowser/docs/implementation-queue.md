# Vibrowser Implementation Queue

> Comprehensive task queue generated from 8-agent audit (2026-02-28)
> Every item is a discrete, implementable unit of work.
> Use `/codex` to implement items in parallel batches of 6.
> Priority: P0 (critical) > P1 (high) > P2 (medium) > P3 (low) > P4 (nice-to-have)

---

## HTML Parser (tokenizer.cpp, tree_builder.cpp)

### Tokenizer States (39/80 implemented — need 41 more)

- [ ] P1 `/codex` — Implement RAWTEXT state for `<style>`, `<xmp>`, `<iframe>`, `<noembed>`, `<noframes>` content
- [ ] P1 `/codex` — Implement Script data state for `<script>` raw text parsing
- [ ] P1 `/codex` — Implement Script data escaped state for `<!-- -->` inside scripts
- [ ] P1 `/codex` — Implement Script data double escaped state for `<script>` inside `<script>`
- [ ] P1 `/codex` — Implement RCDATA state for `<title>`, `<textarea>` content
- [ ] P2 `/codex` — Implement CDATA section state for `<![CDATA[...]]>` in SVG/MathML
- [ ] P2 `/codex` — Implement Before attribute name state (proper whitespace handling)
- [ ] P2 `/codex` — Implement After attribute name state
- [ ] P2 `/codex` — Implement Before attribute value state
- [ ] P2 `/codex` — Implement Attribute value (double-quoted) state
- [ ] P2 `/codex` — Implement Attribute value (single-quoted) state
- [ ] P2 `/codex` — Implement Attribute value (unquoted) state
- [ ] P2 `/codex` — Implement After attribute value (quoted) state
- [ ] P2 `/codex` — Implement Self-closing start tag state
- [ ] P2 `/codex` — Implement Bogus comment state
- [ ] P2 `/codex` — Implement Markup declaration open state (DOCTYPE, comments, CDATA)
- [ ] P2 `/codex` — Implement Comment start state
- [ ] P2 `/codex` — Implement Comment start dash state
- [ ] P2 `/codex` — Implement Comment state
- [ ] P2 `/codex` — Implement Comment less-than sign state
- [ ] P2 `/codex` — Implement Comment end dash state
- [ ] P2 `/codex` — Implement Comment end state
- [ ] P2 `/codex` — Implement Comment end bang state
- [ ] P2 `/codex` — Implement DOCTYPE state
- [ ] P2 `/codex` — Implement Before DOCTYPE name state
- [ ] P2 `/codex` — Implement DOCTYPE name state
- [ ] P2 `/codex` — Implement After DOCTYPE name state
- [ ] P3 `/codex` — Implement After DOCTYPE public keyword state
- [ ] P3 `/codex` — Implement Before DOCTYPE public identifier state
- [ ] P3 `/codex` — Implement DOCTYPE public identifier (double-quoted) state
- [ ] P3 `/codex` — Implement DOCTYPE public identifier (single-quoted) state
- [ ] P3 `/codex` — Implement After DOCTYPE public identifier state
- [ ] P3 `/codex` — Implement Between DOCTYPE public and system identifiers state
- [ ] P3 `/codex` — Implement After DOCTYPE system keyword state
- [ ] P3 `/codex` — Implement Before DOCTYPE system identifier state
- [ ] P3 `/codex` — Implement DOCTYPE system identifier (double-quoted) state
- [ ] P3 `/codex` — Implement DOCTYPE system identifier (single-quoted) state
- [ ] P3 `/codex` — Implement After DOCTYPE system identifier state
- [ ] P3 `/codex` — Implement Bogus DOCTYPE state
- [ ] P3 `/codex` — Implement RAWTEXT less-than sign state
- [ ] P3 `/codex` — Implement RAWTEXT end tag open/name states

### Tree Builder Insertion Modes (13/23 — need 10 more)

- [ ] P1 `/codex` — Implement "in select" insertion mode for `<select>` content
- [ ] P1 `/codex` — Implement "in select in table" insertion mode
- [ ] P1 `/codex` — Implement "in template" insertion mode
- [ ] P2 `/codex` — Implement "in column group" insertion mode
- [ ] P2 `/codex` — Implement "in table text" insertion mode
- [ ] P2 `/codex` — Implement "in frameset" insertion mode (legacy)
- [ ] P2 `/codex` — Implement "after frameset" insertion mode (legacy)
- [ ] P2 `/codex` — Implement "in foreign content" insertion mode (SVG/MathML)
- [ ] P3 `/codex` — Implement "after after body" insertion mode
- [ ] P3 `/codex` — Implement "after after frameset" insertion mode

### Entity Table

- [x] P0 — Entity table expanded to 253 entries (DONE in Round 1)
- [ ] P2 `/codex` — Expand entity table to full 2,231 WHATWG entries (auto-generate from spec JSON)
- [ ] P3 `/codex` — Add entity table unit tests for all 253+ entries

### Parser Features

- [x] P0 — Adoption agency algorithm (DONE in Round 1)
- [ ] P1 `/codex` — Active formatting elements: scope markers at `<applet>`, `<object>`, `<marquee>`, `<template>`
- [ ] P1 `/codex` — Implicit end tags for `<p>`, `<li>`, `<dd>`, `<dt>`, `<option>`, `<optgroup>`
- [ ] P1 `/codex` — Foster parenting: table-related elements appearing in wrong context
- [ ] P1 `/codex` — `<template>` content model: inert document fragment parsing
- [ ] P2 `/codex` — Quirks mode detection from DOCTYPE
- [ ] P2 `/codex` — Limited quirks mode detection
- [ ] P2 `/codex` — `document.write()` support: tokenizer reentrancy
- [ ] P2 `/codex` — `<form>` element pointer tracking per spec
- [ ] P2 `/codex` — Fragment parsing algorithm for `innerHTML`
- [ ] P3 `/codex` — `<noscript>` content parsing when scripting is disabled
- [ ] P3 `/codex` — `<plaintext>` PLAINTEXT state switching
- [ ] P3 `/codex` — Input stream preprocessing (BOM detection, encoding sniffing)
- [ ] P3 `/codex` — Parse error reporting and recovery

---

## CSS Parser & Style Resolver

### Missing CSS Properties

- [ ] P1 `/codex` — `animation` shorthand runtime (keyframes, timing, iteration, direction)
- [ ] P1 `/codex` — `animation-name` + `@keyframes` rule linking
- [ ] P1 `/codex` — `animation-duration`, `animation-delay`, `animation-timing-function`
- [ ] P1 `/codex` — `animation-iteration-count`, `animation-direction`, `animation-fill-mode`
- [ ] P1 `/codex` — `animation-play-state` (running/paused)
- [ ] P1 `/codex` — `transition-property`, `transition-duration`, `transition-timing-function`, `transition-delay`
- [ ] P1 `/codex` — Transition runtime engine: interpolation between computed values
- [ ] P1 `/codex` — `@font-face` font loading: fetch font file, register with system
- [ ] P2 `/codex` — `@media` range syntax: `@media (width >= 600px)`, `@media (400px <= width <= 800px)`
- [ ] P2 `/codex` — `@media` unit conversion (rem, em, vw, vh in media queries)
- [ ] P2 `/codex` — `@container` queries: container type, container name
- [ ] P2 `/codex` — `@container` size queries: `min-width`, `max-width`, `min-height`, `max-height`
- [ ] P2 `/codex` — `@scope` rule parsing and cascade integration
- [ ] P2 `/codex` — `@starting-style` rule for entry animations
- [ ] P2 `/codex` — CSS custom properties: `@property` rule for typed custom properties
- [ ] P2 `/codex` — `env()` function: `safe-area-inset-top/right/bottom/left`
- [ ] P2 `/codex` — `color-scheme: light dark` cascade integration
- [ ] P2 `/codex` — `forced-colors` media query
- [ ] P2 `/codex` — `prefers-reduced-motion` media query
- [ ] P2 `/codex` — `prefers-color-scheme` media query improvements
- [ ] P3 `/codex` — `@page` rule for print styles
- [ ] P3 `/codex` — `@font-feature-values` rule
- [ ] P3 `/codex` — `@counter-style` rule
- [ ] P3 `/codex` — Selector: `:has()` pseudo-class
- [ ] P3 `/codex` — Selector: `:is()` / `:where()` pseudo-classes
- [ ] P3 `/codex` — Selector: `:not()` with complex selectors
- [ ] P3 `/codex` — Selector: `:nth-child(An+B of S)` with selector argument
- [ ] P3 `/codex` — Selector: `::part()` for Shadow DOM
- [ ] P3 `/codex` — Selector: `::slotted()` for Web Components
- [ ] P4 `/codex` — `@layer` anonymous layers
- [ ] P4 `/codex` — Nesting: double-nesting (nested rules inside nested rules)
- [ ] P4 `/codex` — `color()` function: rec2020, xyz color spaces

### CSS Values & Units

- [ ] P2 `/codex` — `anchor()` function for CSS Anchor Positioning
- [ ] P2 `/codex` — `round()`, `mod()`, `rem()` math functions
- [ ] P2 `/codex` — `sign()`, `abs()` math functions
- [ ] P2 `/codex` — `color-contrast()` function
- [ ] P2 `/codex` — `ic` unit (ideographic character width)
- [ ] P2 `/codex` — `cqi`, `cqb`, `cqw`, `cqh` container query units
- [ ] P3 `/codex` — `attr()` function with type and fallback
- [ ] P3 `/codex` — `toggle()` function for cycling values
- [ ] P4 `/codex` — `ray()` function for offset-path

---

## Layout Engine (layout_engine.cpp, box.h)

### Block Layout

- [ ] P0 `/codex` — Block Formatting Context (BFC) establishment and containment
- [ ] P0 `/codex` — Parent-child margin collapsing (first/last child margins)
- [ ] P0 `/codex` — Adjacent sibling margin collapsing improvements
- [ ] P1 `/codex` — Empty block margin collapsing
- [ ] P1 `/codex` — Float containment within BFC boundaries
- [ ] P1 `/codex` — `clear` property: clear left/right/both floats correctly
- [ ] P2 `/codex` — `display: flow-root` (creates BFC)
- [ ] P2 `/codex` — `display: contents` (element removed from layout, children promoted)
- [ ] P2 `/codex` — Writing modes: `writing-mode: vertical-rl`, `vertical-lr`
- [ ] P2 `/codex` — Logical properties: `margin-inline-start`, `padding-block-end`, etc.
- [ ] P3 `/codex` — `break-before`, `break-after`, `break-inside` for fragmentation
- [ ] P3 `/codex` — `orphans` and `widows` for page/column breaks

### Inline Layout

- [ ] P0 `/codex` — Proper line box construction with baseline alignment
- [ ] P0 `/codex` — `vertical-align` precision (top, bottom, middle, sub, super, length, percentage)
- [ ] P1 `/codex` — Inline-block baseline alignment
- [ ] P1 `/codex` — `text-align: justify` with inter-word spacing
- [ ] P1 `/codex` — `word-break: break-word` vs `overflow-wrap: break-word` distinction
- [ ] P1 `/codex` — `hyphens: auto` with soft hyphen support
- [ ] P2 `/codex` — `initial-letter` property
- [ ] P2 `/codex` — `ruby-position` and `ruby-align` for CJK
- [ ] P2 `/codex` — `text-combine-upright` for vertical text
- [ ] P3 `/codex` — Bidi algorithm (Unicode Bidirectional Algorithm)
- [ ] P3 `/codex` — Line breaking: Unicode line break algorithm (UAX #14)

### Flex Layout

- [ ] P1 `/codex` — `flex-basis: content` sizing
- [ ] P1 `/codex` — Flex item `min-content` and `max-content` intrinsic sizing
- [ ] P1 `/codex` — `gap` property in flex containers (row-gap, column-gap)
- [ ] P2 `/codex` — `order` property implementation
- [ ] P2 `/codex` — `align-self: stretch` default behavior
- [ ] P2 `/codex` — Cross-axis sizing with `align-items`/`align-self`
- [ ] P2 `/codex` — Flex line wrapping with `align-content`
- [ ] P3 `/codex` — Flex item `visibility: collapse`

### Grid Layout

- [ ] P1 `/codex` — Grid track sizing algorithm (min-content, max-content, fractional)
- [ ] P1 `/codex` — `auto-fill` and `auto-fit` repeat
- [ ] P1 `/codex` — `minmax()` function in grid tracks
- [ ] P1 `/codex` — Named grid lines and areas
- [ ] P1 `/codex` — `grid-auto-rows` and `grid-auto-columns`
- [ ] P1 `/codex` — `grid-auto-flow: dense` packing algorithm
- [ ] P2 `/codex` — Subgrid (`grid-template-rows: subgrid`)
- [ ] P2 `/codex` — `gap` property in grid (row-gap, column-gap)
- [ ] P2 `/codex` — Grid item placement: span, named line references
- [ ] P2 `/codex` — `align-items`, `justify-items`, `place-items` in grid
- [ ] P2 `/codex` — `align-content`, `justify-content`, `place-content` in grid
- [ ] P3 `/codex` — Masonry layout (experimental)

### Table Layout

- [ ] P1 `/codex` — `table-layout: auto` column width algorithm
- [ ] P1 `/codex` — `border-collapse: collapse` border conflict resolution
- [ ] P2 `/codex` — `border-spacing` in separated borders model
- [ ] P2 `/codex` — `empty-cells: show/hide` styling
- [ ] P2 `/codex` — Table caption positioning (`caption-side: bottom`)
- [ ] P3 `/codex` — Table percentage width distribution

### Positioned Layout

- [ ] P1 `/codex` — `position: sticky` threshold-based scrolling
- [ ] P1 `/codex` — `position: fixed` relative to viewport (not initial containing block)
- [ ] P2 `/codex` — Containing block for `position: absolute` based on nearest positioned ancestor
- [ ] P2 `/codex` — `inset` shorthand property
- [ ] P3 `/codex` — CSS Anchor Positioning

### Multi-column Layout

- [ ] P2 `/codex` — `column-width: auto` with `column-count`
- [ ] P2 `/codex` — `column-fill: balance` vs `auto`
- [ ] P2 `/codex` — `column-span: all` spanning elements
- [ ] P3 `/codex` — `column-rule` styling (width, style, color)
- [ ] P3 `/codex` — Column overflow and scrolling

### Overflow & Scrolling

- [ ] P0 `/codex` — Scroll container with scrollbar rendering
- [ ] P0 `/codex` — `overflow-y: auto` — show scrollbar only when needed
- [ ] P0 `/codex` — Scroll event handling (mousewheel, trackpad)
- [ ] P1 `/codex` — `overflow-x` and `overflow-y` independent control
- [ ] P1 `/codex` — `scroll-snap-type` and `scroll-snap-align`
- [ ] P1 `/codex` — `scroll-behavior: smooth` animation
- [ ] P2 `/codex` — `scrollbar-width: thin/none`
- [ ] P2 `/codex` — `scrollbar-color` customization
- [ ] P2 `/codex` — `overscroll-behavior` (contain, none)
- [ ] P3 `/codex` — `scroll-padding` and `scroll-margin`
- [ ] P3 `/codex` — `scroll-timeline` for scroll-linked animations

### Sizing

- [ ] P1 `/codex` — `min-content` and `max-content` as width/height values
- [ ] P1 `/codex` — `fit-content` sizing
- [ ] P2 `/codex` — `contain-intrinsic-size` for content-visibility
- [ ] P2 `/codex` — Intrinsic sizing for replaced elements (aspect ratio)
- [ ] P3 `/codex` — `field-sizing: content` for form controls

---

## Paint Pipeline (painter.cpp, software_renderer.cpp, render_pipeline.cpp)

### Stacking & Compositing

- [x] P0 — Proper stacking contexts (DONE in Round 1)
- [ ] P1 `/codex` — Compositing layers for `position: fixed` elements
- [ ] P1 `/codex` — `will-change: transform` layer promotion
- [ ] P2 `/codex` — Compositing layers for animated elements
- [ ] P2 `/codex` — `contain: paint` isolation
- [ ] P3 `/codex` — Layer squashing for memory efficiency

### Per-Corner Border Radius

- [ ] P1 `/codex` — Per-corner border-radius in display list (currently single radius)
- [ ] P1 `/codex` — `border-top-left-radius`, `border-top-right-radius` etc. in painter
- [ ] P1 `/codex` — Elliptical border-radius (x/y radii)
- [ ] P2 `/codex` — Per-corner clipping for overflow:hidden with border-radius

### Rendering Features

- [ ] P1 `/codex` — `outline-offset` rendering
- [ ] P1 `/codex` — `box-decoration-break: clone` for fragmented boxes
- [ ] P2 `/codex` — `background-clip: text` (text-shaped background)
- [ ] P2 `/codex` — Multiple backgrounds rendering
- [ ] P2 `/codex` — `background-attachment: fixed` for parallax
- [ ] P2 `/codex` — `mask-image` with URL images (not just gradients)
- [ ] P2 `/codex` — `shape-outside` for float wrapping
- [ ] P2 `/codex` — `paint()` CSS Houdini paint API
- [ ] P3 `/codex` — `image-set()` responsive images in CSS
- [ ] P3 `/codex` — `cross-fade()` function
- [ ] P3 `/codex` — `element()` function (render element as image)
- [ ] P3 `/codex` — Print rendering (`@page`, page breaks, headers/footers)

### GPU Compositing

- [ ] P1 `/codex` — Metal or Core Animation compositing layer system
- [ ] P1 `/codex` — Texture management for composited layers
- [ ] P1 `/codex` — Damage tracking (dirty rectangles) for partial repaint
- [ ] P2 `/codex` — GPU-accelerated transforms
- [ ] P2 `/codex` — GPU-accelerated opacity blending
- [ ] P2 `/codex` — GPU-accelerated filter effects
- [ ] P3 `/codex` — Offscreen canvas rendering to Metal texture
- [ ] P3 `/codex` — Video frame compositing

### Incremental Rendering

- [ ] P0 `/codex` — Break monolithic `render_html()` into parse/style/layout/paint stages
- [ ] P0 `/codex` — Progressive rendering: show content as it loads
- [ ] P1 `/codex` — Dirty flag propagation: only re-layout changed subtrees
- [ ] P1 `/codex` — Style recalculation: only recompute changed elements
- [ ] P1 `/codex` — `requestAnimationFrame` integration with render loop
- [ ] P2 `/codex` — Off-main-thread layout computation
- [ ] P2 `/codex` — Tiled rendering for large pages
- [ ] P3 `/codex` — Content-visibility: auto lazy rendering

---

## JavaScript Engine (js_engine.cpp, js_dom_bindings.cpp, js_window.cpp)

### DOM API

- [x] P0 — HTMLInputElement.value, .checked, .type properties (DONE in Round 1)
- [x] P0 — Event default actions (DONE in Round 1)
- [ ] P0 `/codex` — `element.innerHTML` getter/setter (currently only getter)
- [ ] P0 `/codex` — `element.outerHTML` getter/setter
- [ ] P0 `/codex` — `element.insertAdjacentHTML()` method
- [ ] P0 `/codex` — `element.closest()` method
- [ ] P0 `/codex` — `element.matches()` method
- [ ] P1 `/codex` — `element.getBoundingClientRect()` — return layout position/size
- [ ] P1 `/codex` — `element.offsetWidth/offsetHeight/offsetTop/offsetLeft`
- [ ] P1 `/codex` — `element.clientWidth/clientHeight/clientTop/clientLeft`
- [ ] P1 `/codex` — `element.scrollWidth/scrollHeight/scrollTop/scrollLeft`
- [ ] P1 `/codex` — `element.scrollIntoView()` method
- [ ] P1 `/codex` — `element.focus()` and `element.blur()` methods
- [ ] P1 `/codex` — `document.createDocumentFragment()`
- [ ] P1 `/codex` — `document.createComment()`
- [ ] P1 `/codex` — `document.createTextNode()` improvements
- [ ] P1 `/codex` — `Node.cloneNode(deep)` method
- [ ] P1 `/codex` — `Node.contains()` method
- [ ] P1 `/codex` — `Node.compareDocumentPosition()` method
- [ ] P1 `/codex` — `Node.normalize()` method
- [ ] P2 `/codex` — `NodeList` live collection behavior
- [ ] P2 `/codex` — `HTMLCollection` live collection behavior
- [ ] P2 `/codex` — `NamedNodeMap` for attributes
- [ ] P2 `/codex` — `DOMTokenList` improvements (toggle with force, replace, supports)
- [ ] P2 `/codex` — `MutationObserver` real implementation (currently stub)
- [ ] P2 `/codex` — `IntersectionObserver` implementation
- [ ] P2 `/codex` — `ResizeObserver` implementation
- [ ] P2 `/codex` — `Range` and `Selection` API
- [ ] P2 `/codex` — `TreeWalker` and `NodeIterator`
- [ ] P3 `/codex` — `DOMParser` and `XMLSerializer`
- [ ] P3 `/codex` — `CustomElementRegistry` (define, get, whenDefined)
- [ ] P3 `/codex` — `ShadowDOM` (attachShadow, shadowRoot)

### Window & Navigation

- [ ] P0 `/codex` — `window.location` full spec (assign, replace, reload, origin, searchParams)
- [ ] P1 `/codex` — `History` API (pushState, replaceState, popstate event, back, forward, go)
- [ ] P1 `/codex` — `window.open()` / `window.close()`
- [ ] P1 `/codex` — `window.scrollTo()` / `window.scrollBy()`
- [ ] P1 `/codex` — `window.innerWidth` / `window.innerHeight` from actual viewport
- [ ] P1 `/codex` — `window.devicePixelRatio`
- [ ] P2 `/codex` — `window.matchMedia()` MediaQueryList
- [ ] P2 `/codex` — `window.getComputedStyle()` — return actual computed values
- [ ] P2 `/codex` — `window.requestIdleCallback()`
- [ ] P2 `/codex` — `window.crypto.getRandomValues()`
- [ ] P2 `/codex` — `window.crypto.subtle` (SubtleCrypto API)
- [ ] P2 `/codex` — `navigator.clipboard` API
- [ ] P2 `/codex` — `navigator.geolocation` API
- [ ] P2 `/codex` — `navigator.permissions` API
- [ ] P3 `/codex` — `postMessage` cross-origin messaging
- [ ] P3 `/codex` — `BroadcastChannel` API
- [ ] P3 `/codex` — `SharedWorker` support

### Event System

- [ ] P0 `/codex` — Keyboard events (keydown, keyup, keypress) with key/code properties
- [ ] P0 `/codex` — Mouse events (mousedown, mouseup, mousemove, mouseenter, mouseleave)
- [ ] P0 `/codex` — Input events (input, change, select) for form controls
- [ ] P1 `/codex` — Touch events (touchstart, touchmove, touchend, touchcancel)
- [ ] P1 `/codex` — Pointer events (pointerdown, pointermove, pointerup)
- [ ] P1 `/codex` — Wheel event (scroll wheel with deltaX, deltaY, deltaZ)
- [ ] P1 `/codex` — Focus events (focusin, focusout, focus, blur) with relatedTarget
- [ ] P1 `/codex` — Drag events (dragstart, drag, dragover, drop, dragend)
- [ ] P2 `/codex` — Composition events (compositionstart, compositionupdate, compositionend) for IME
- [ ] P2 `/codex` — Animation events (animationstart, animationend, animationiteration)
- [ ] P2 `/codex` — Transition events (transitionstart, transitionend, transitionrun, transitioncancel)
- [ ] P2 `/codex` — Resize event with ResizeObserverEntry
- [ ] P2 `/codex` — `CustomEvent` with detail property
- [ ] P3 `/codex` — `AbortController` / `AbortSignal`
- [ ] P3 `/codex` — `EventTarget` as standalone class

### ES Modules

- [ ] P0 `/codex` — `<script type="module">` — stop skipping, start executing
- [ ] P0 `/codex` — Static `import` declarations (resolve, fetch, parse, link, evaluate)
- [ ] P0 `/codex` — `export` declarations (default, named)
- [ ] P1 `/codex` — Dynamic `import()` expression
- [ ] P1 `/codex` — Module scope isolation (no global leaking)
- [ ] P1 `/codex` — Module caching (each URL loaded once)
- [ ] P2 `/codex` — `import.meta.url`
- [ ] P2 `/codex` — `import.meta.resolve()`
- [ ] P2 `/codex` — Top-level await
- [ ] P3 `/codex` — Import maps (`<script type="importmap">`)
- [ ] P3 `/codex` — Module workers

### Web APIs

- [ ] P1 `/codex` — `Canvas 2D API`: fillRect, strokeRect, clearRect, fillText, strokeText
- [ ] P1 `/codex` — `Canvas 2D API`: paths (beginPath, moveTo, lineTo, arc, closePath, fill, stroke)
- [ ] P1 `/codex` — `Canvas 2D API`: gradients (createLinearGradient, createRadialGradient)
- [ ] P1 `/codex` — `Canvas 2D API`: images (drawImage, createImageData, getImageData, putImageData)
- [ ] P1 `/codex` — `Canvas 2D API`: transformations (translate, rotate, scale, transform, setTransform)
- [ ] P2 `/codex` — `Canvas 2D API`: text measurement (measureText)
- [ ] P2 `/codex` — `Canvas 2D API`: compositing (globalAlpha, globalCompositeOperation)
- [ ] P2 `/codex` — `Canvas 2D API`: shadows
- [ ] P2 `/codex` — `Canvas 2D API`: patterns (createPattern)
- [ ] P2 `/codex` — `FormData` API
- [ ] P2 `/codex` — `Blob` and `File` API
- [ ] P2 `/codex` — `FileReader` API
- [ ] P2 `/codex` — `createObjectURL` / `revokeObjectURL` (currently stubs)
- [ ] P2 `/codex` — `Intl` API: NumberFormat, DateTimeFormat, Collator
- [ ] P2 `/codex` — `OffscreenCanvas` API
- [ ] P3 `/codex` — `Web Animations API` (element.animate())
- [ ] P3 `/codex` — `Web Audio API` basics
- [ ] P3 `/codex` — `MediaStream` API
- [ ] P3 `/codex` — `WebRTC` basics (RTCPeerConnection)
- [ ] P3 `/codex` — `IndexedDB` API
- [ ] P3 `/codex` — `Cache` API (for Service Workers)
- [ ] P4 `/codex` — `WebGL` context creation
- [ ] P4 `/codex` — `WebGPU` context creation
- [ ] P4 `/codex` — `Payment Request API`
- [ ] P4 `/codex` — `Web Speech API`

---

## Networking (http_client.cpp, tls_socket.cpp, connection_pool.cpp)

### HTTP/2

- [ ] P1 `/codex` — HPACK header compression (encode/decode)
- [ ] P1 `/codex` — HTTP/2 frame parsing (DATA, HEADERS, SETTINGS, WINDOW_UPDATE, etc.)
- [ ] P1 `/codex` — HTTP/2 stream multiplexing
- [ ] P1 `/codex` — HTTP/2 ALPN negotiation during TLS handshake
- [ ] P2 `/codex` — HTTP/2 server push handling
- [ ] P2 `/codex` — HTTP/2 flow control (window updates)
- [ ] P2 `/codex` — HTTP/2 priority/weight
- [ ] P3 `/codex` — HTTP/3 (QUIC) support

### Connection Management

- [x] P0 — HTTP Keep-Alive + ConnectionPool (DONE in Round 1)
- [ ] P1 `/codex` — TLS session resumption for HTTPS keep-alive
- [ ] P1 `/codex` — Connection pool idle timeout (evict stale connections)
- [ ] P1 `/codex` — DNS caching with TTL
- [ ] P2 `/codex` — Happy Eyeballs (RFC 6555): race IPv4/IPv6 connections
- [ ] P2 `/codex` — Pre-connect for known origins
- [ ] P2 `/codex` — `<link rel="preconnect">` support
- [ ] P3 `/codex` — SOCKS proxy support
- [ ] P3 `/codex` — HTTP proxy support (CONNECT method)

### Resource Loading

- [ ] P0 `/codex` — Parallel resource fetching (images, CSS, JS in parallel)
- [ ] P0 `/codex` — Resource prioritization (CSS > JS > images)
- [ ] P1 `/codex` — `<link rel="preload">` resource hints
- [ ] P1 `/codex` — `<link rel="prefetch">` for next navigation
- [ ] P1 `/codex` — `<link rel="dns-prefetch">` DNS resolution
- [ ] P1 `/codex` — Brotli decompression (Content-Encoding: br)
- [ ] P2 `/codex` — `<script async>` non-blocking script loading
- [ ] P2 `/codex` — `<script defer>` deferred script execution
- [ ] P2 `/codex` — Subresource Integrity (SRI) hash verification
- [ ] P2 `/codex` — `Content-Security-Policy` enforcement
- [ ] P2 `/codex` — `Referrer-Policy` header handling
- [ ] P3 `/codex` — `<link rel="modulepreload">`
- [ ] P3 `/codex` — 103 Early Hints support
- [ ] P3 `/codex` — Range requests for partial content (206)

### Cookie Management

- [ ] P1 `/codex` — `SameSite=Strict` enforcement
- [ ] P1 `/codex` — `SameSite=Lax` enforcement (default)
- [ ] P1 `/codex` — `SameSite=None; Secure` cross-site cookies
- [ ] P2 `/codex` — Cookie partitioning (CHIPS)
- [ ] P2 `/codex` — Cookie domain matching per RFC 6265bis
- [ ] P2 `/codex` — Cookie expiration/Max-Age handling
- [ ] P3 `/codex` — `Partitioned` cookie attribute

### Security

- [ ] P1 `/codex` — HTTPS certificate validation (currently trusts all)
- [ ] P1 `/codex` — HSTS (HTTP Strict Transport Security)
- [ ] P1 `/codex` — Mixed content blocking (HTTP resources on HTTPS pages)
- [ ] P2 `/codex` — Certificate Transparency
- [ ] P2 `/codex` — Public Key Pinning (HPKP) — deprecated but some sites use
- [ ] P3 `/codex` — OCSP stapling

---

## DOM & Event System (dom/, js_dom_bindings.cpp)

### DOM Tree

- [ ] P1 `/codex` — Unify dual DOM: merge C++ dom::Node and SimpleNode into one tree
- [ ] P1 `/codex` — Typed HTML elements: HTMLFormElement with submit(), reset(), elements
- [ ] P1 `/codex` — Typed HTML elements: HTMLSelectElement with options, selectedOptions
- [ ] P1 `/codex` — Typed HTML elements: HTMLTableElement with rows, insertRow, deleteRow
- [ ] P1 `/codex` — Typed HTML elements: HTMLCanvasElement with getContext, toDataURL
- [ ] P2 `/codex` — Typed HTML elements: HTMLMediaElement (play, pause, currentTime, duration)
- [ ] P2 `/codex` — Typed HTML elements: HTMLDialogElement (show, showModal, close)
- [ ] P2 `/codex` — `DocumentFragment` support
- [ ] P2 `/codex` — `Attr` node type
- [ ] P3 `/codex` — `ProcessingInstruction` node type
- [ ] P3 `/codex` — `CDATASection` node type

### Shadow DOM

- [ ] P2 `/codex` — `element.attachShadow({mode: 'open'})`
- [ ] P2 `/codex` — Shadow root rendering
- [ ] P2 `/codex` — `<slot>` element content distribution
- [ ] P2 `/codex` — `::slotted()` pseudo-element styling
- [ ] P2 `/codex` — Shadow DOM event retargeting
- [ ] P3 `/codex` — `element.attachShadow({mode: 'closed'})`
- [ ] P3 `/codex` — Declarative Shadow DOM (`<template shadowrootmode="open">`)
- [ ] P3 `/codex` — Adopted stylesheets

### Custom Elements

- [ ] P2 `/codex` — `customElements.define()` registration
- [ ] P2 `/codex` — Custom element lifecycle callbacks (connectedCallback, disconnectedCallback)
- [ ] P2 `/codex` — `attributeChangedCallback` with observed attributes
- [ ] P2 `/codex` — Custom element upgrade
- [ ] P3 `/codex` — `customElements.whenDefined()` promise
- [ ] P3 `/codex` — `is` attribute for customized built-in elements

---

## Platform & Shell (browser_window.mm, render_view.mm, main.mm)

### macOS Integration

- [ ] P1 `/codex` — Native scrollbar rendering (NSScroller or custom)
- [ ] P1 `/codex` — Text input with IME support (NSTextInputClient)
- [ ] P1 `/codex` — Right-click context menu (copy, paste, inspect, etc.)
- [ ] P1 `/codex` — Keyboard shortcuts (Cmd+A select all, Cmd+C copy, Cmd+V paste)
- [ ] P2 `/codex` — Text selection with mouse drag
- [ ] P2 `/codex` — Cursor changes per element (pointer, text, move, etc.)
- [ ] P2 `/codex` — Drag and drop support
- [ ] P2 `/codex` — File picker dialog for `<input type="file">`
- [ ] P2 `/codex` — Color picker dialog for `<input type="color">`
- [ ] P2 `/codex` — Date/time picker for `<input type="date">`
- [ ] P2 `/codex` — Print dialog
- [ ] P3 `/codex` — Full-screen mode
- [ ] P3 `/codex` — Touch Bar support
- [ ] P3 `/codex` — Handoff / Universal Clipboard

### Developer Tools

- [ ] P1 `/codex` — Element inspector (highlight on hover, show box model)
- [ ] P1 `/codex` — Console panel (console.log/warn/error output)
- [ ] P2 `/codex` — Network panel (request/response viewer)
- [ ] P2 `/codex` — DOM tree viewer
- [ ] P2 `/codex` — CSS computed styles panel
- [ ] P2 `/codex` — JavaScript debugger (breakpoints via QuickJS debug API)
- [ ] P3 `/codex` — Performance profiler
- [ ] P3 `/codex` — Memory profiler
- [ ] P3 `/codex` — Application panel (cookies, localStorage, sessionStorage)
- [ ] P3 `/codex` — Source viewer with syntax highlighting

### Rendering Pipeline

- [ ] P0 `/codex` — `requestAnimationFrame` callback integration
- [ ] P0 `/codex` — Frame-rate limiting (60fps target with vsync)
- [ ] P1 `/codex` — Async image decoding (don't block main thread)
- [ ] P1 `/codex` — Lazy image loading (`loading="lazy"`)
- [ ] P1 `/codex` — `<img srcset>` and `<picture>` responsive image selection
- [ ] P2 `/codex` — Animated GIF/APNG/WebP playback
- [ ] P2 `/codex` — Video frame rendering (AVFoundation integration)
- [ ] P2 `/codex` — Audio playback (AVAudioPlayer integration)
- [ ] P3 `/codex` — PDF rendering (`<embed type="application/pdf">`)
- [ ] P3 `/codex` — SVG animation (SMIL)

### Multi-process Architecture

- [ ] P2 `/codex` — Separate renderer process per tab
- [ ] P2 `/codex` — IPC between browser process and renderer (existing IPC infrastructure)
- [ ] P2 `/codex` — GPU process for compositing
- [ ] P3 `/codex` — Network process isolation
- [ ] P3 `/codex` — Plugin/extension process

---

## Testing & Quality

### Test Infrastructure

- [ ] P2 `/codex` — Web Platform Tests (WPT) runner integration
- [ ] P2 `/codex` — CSS Test Suite integration
- [ ] P2 `/codex` — Acid1/Acid2/Acid3 compliance testing
- [ ] P2 `/codex` — Visual regression testing (screenshot comparison)
- [ ] P2 `/codex` — Performance benchmarks (Speedometer, MotionMark, JetStream)
- [ ] P3 `/codex` — Fuzzing infrastructure (libFuzzer for parser)
- [ ] P3 `/codex` — Address/memory sanitizer CI runs

### Conformance

- [ ] P2 `/codex` — html5lib test suite compliance
- [ ] P2 `/codex` — CSS-in-JS library testing (styled-components, emotion)
- [ ] P2 `/codex` — React rendering compatibility
- [ ] P2 `/codex` — jQuery compatibility
- [ ] P3 `/codex` — ECMAScript Test262 compliance (via QuickJS)
- [ ] P3 `/codex` — URL parser WPT compliance

---

## Architecture Refactoring

### DOM Unification

- [ ] P1 `/codex` — Phase 1: Add layout info to SimpleNode (position, size, computed styles)
- [ ] P1 `/codex` — Phase 2: Remove C++ dom::Node hierarchy (unused)
- [ ] P1 `/codex` — Phase 3: Direct SimpleNode → LayoutNode bridge
- [ ] P2 `/codex` — Phase 4: Incremental style recalculation on DOM mutation

### Render Pipeline Decomposition

- [ ] P0 `/codex` — Extract HTML parsing from render_html() into separate stage
- [ ] P0 `/codex` — Extract CSS resolution from render_html() into separate stage
- [ ] P0 `/codex` — Extract layout from render_html() into separate stage
- [ ] P0 `/codex` — Extract painting from render_html() into separate stage
- [ ] P1 `/codex` — Extract rasterization from render_html() into separate stage
- [ ] P1 `/codex` — Add dirty tracking between stages

### LayoutNode Refactoring

- [ ] P2 `/codex` — Split LayoutNode into: StyleData (computed values)
- [ ] P2 `/codex` — Split LayoutNode into: GeometryData (position, size, margins)
- [ ] P2 `/codex` — Split LayoutNode into: PaintData (background, border, effects)
- [ ] P2 `/codex` — Split LayoutNode into: TreeData (children, parent, siblings)
- [ ] P2 `/codex` — Reduce per-node memory from ~4-5KB to ~1KB

### Event Loop Integration

- [ ] P1 `/codex` — Wire up custom EventLoop to NSRunLoop
- [ ] P1 `/codex` — Task queues: microtask, macrotask, animation frame
- [ ] P1 `/codex` — Wire up ThreadPool for parallel resource loading
- [ ] P2 `/codex` — Idle callback queue (requestIdleCallback)
- [ ] P2 `/codex` — Remove or repurpose disconnected EventLoop/ThreadPool

---

## Statistics

| Category | P0 | P1 | P2 | P3 | P4 | Total |
|----------|-----|-----|-----|-----|-----|-------|
| HTML Parser | 2 | 8 | 22 | 18 | 0 | 50 |
| CSS | 0 | 8 | 22 | 12 | 3 | 45 |
| Layout | 4 | 24 | 28 | 12 | 0 | 68 |
| Paint | 2 | 9 | 14 | 7 | 0 | 32 |
| JavaScript | 6 | 28 | 42 | 20 | 4 | 100 |
| Networking | 2 | 14 | 18 | 10 | 0 | 44 |
| DOM | 0 | 8 | 16 | 8 | 0 | 32 |
| Platform | 2 | 10 | 18 | 12 | 0 | 42 |
| Testing | 0 | 0 | 7 | 3 | 0 | 10 |
| Architecture | 4 | 8 | 8 | 0 | 0 | 20 |
| **TOTAL** | **22** | **117** | **195** | **102** | **7** | **443** |

*Each item is a discrete `/codex` task. Running 6 in parallel = ~74 rounds to complete all.*
