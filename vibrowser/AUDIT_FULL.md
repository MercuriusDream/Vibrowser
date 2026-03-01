# Vibrowser Full Spec Compliance Audit

Generated: 2026-03-01 | 20 parallel Haiku audit agents

---

## Table of Contents
1. [CSS Properties](#css-properties)
2. [CSS Selectors](#css-selectors)
3. [CSS At-Rules & Functions](#css-at-rules--functions)
4. [CSS Values & Units](#css-values--units)
5. [CSS Layout Algorithms](#css-layout-algorithms)
6. [CSS Paint & Visual Rendering](#css-paint--visual-rendering)
7. [HTML Elements](#html-elements)
8. [HTML Parsing](#html-parsing)
9. [JS DOM Core API](#js-dom-core-api)
10. [JS Events API](#js-events-api)
11. [JS Web APIs](#js-web-apis)
12. [JS Language Features (ES2015+)](#js-language-features)
13. [Networking & Protocols](#networking--protocols)
14. [Image & Media Support](#image--media-support)

---

## CSS Properties

### A-B Properties

- [ ] align-content — partial — Parsed/stored but grid layout does NOT respect it
- [ ] align-items — partial — Works in flexbox; grid does NOT respect it
- [ ] align-self — partial — Works in flexbox; grid does NOT respect it
- [ ] all — partial — Stores string but does NOT reset all properties
- [ ] animation — partial — Runs forward from start; play-state changes at runtime NOT supported
- [ ] animation-delay — partial — Complex delay with multiple animations NOT tested
- [ ] animation-direction — partial — alternate-reverse may have bugs
- [ ] animation-duration — partial — Duration 0 never starts (should immediately apply final values)
- [ ] animation-fill-mode — partial — forwards may not persist final frame
- [ ] animation-iteration-count — partial — Infinite animations may have memory issues
- [ ] animation-name — partial — No animationstart/animationend events fired
- [ ] animation-play-state — partial — Parsed but NEVER used at runtime
- [ ] animation-timing-function — partial — Custom timing on multiple animations not verified
- [ ] appearance — partial — Parsed but NO native platform styles rendered
- [ ] aspect-ratio — partial — Interactive resize doesn't maintain it
- [ ] backdrop-filter — partial — Only basic filters; complex compositing may be buggy
- [ ] backface-visibility — partial — Parsed but NOT rendered; 3D transforms don't hide back faces
- [ ] background — partial — Some subproperties not rendered
- [ ] background-attachment — partial — Parsed but fixed background doesn't stay fixed during scroll
- [ ] background-blend-mode — partial — Works for gradients only, not solid colors or images
- [ ] background-clip — partial — text clipping NOT implemented
- [x] background-color — implemented
- [ ] background-image — partial — URL-based images rendered but some edge cases missing
- [ ] background-origin — partial — Only affects gradient/solid positioning
- [ ] background-position — partial — Works for gradients; edge cases with images
- [ ] background-repeat — partial — Works for gradients
- [ ] background-size — partial — Works but background images edge cases
- [x] block-size — implemented (maps to height)
- [ ] border — partial — Individual side shorthands (border-top, border-bottom etc.) MISSING
- [ ] border-block — partial — Logical properties work
- [ ] border-bottom — missing — Shorthand does NOT parse; longhands work
- [ ] border-collapse — partial — Limited testing with tables
- [ ] border-image — partial — Gradient sources work; URL-based images NOT fetched
- [ ] border-image-outset — partial — May not affect final painted bounds
- [ ] border-image-repeat — partial — Only gradient-based
- [ ] border-image-slice — partial — 9-slice for images NOT fully implemented
- [ ] border-image-source — partial — URL-based images NOT loaded
- [ ] border-image-width — partial — NOT properly applied to border width
- [ ] border-inline — missing — Shorthand does NOT parse; longhands work
- [ ] border-left — missing — Shorthand does NOT parse; longhands work
- [ ] border-radius — partial — Elliptical radii NOT supported (circular only)
- [ ] border-right — missing — Shorthand does NOT parse; longhands work
- [ ] border-spacing — partial — May not affect non-collapsed borders
- [ ] border-style — partial — groove/ridge/inset/outset have simplified rendering
- [ ] border-top — missing — Shorthand does NOT parse; longhands work
- [ ] bottom — partial — Percentage relative to viewport, not container
- [ ] box-decoration-break — partial — Parsed but NOT used in fragmentation
- [ ] box-shadow — partial — Inset may not properly layer with outset
- [x] box-sizing — implemented
- [ ] break-after — partial — Parsed but NO fragmentation logic
- [ ] break-before — partial — Parsed but NO fragmentation logic
- [ ] break-inside — partial — Parsed but NO fragmentation logic

### C-D Properties

- [ ] caption-side — partial — Parsed but NOT rendered in paint pipeline
- [ ] caret-color — partial — Only for input/textarea; missing other editable elements
- [x] clear — implemented
- [ ] clip — missing — Legacy CSS 2 clip:rect() not implemented
- [ ] clip-path — partial — Shapes parsed but NOT rendered in painter
- [ ] clip-rule — partial — SVG property parsed but no integration
- [x] color — implemented
- [ ] color-scheme — partial — Used for input styling; no media query/viewport color
- [ ] column-count — partial — Parsed; column rules work but NO column text distribution
- [ ] column-fill — partial — Parsed but NOT used
- [ ] column-gap — partial — Parsed; rule spacing calculated but NO text distribution
- [ ] column-rule-color — implemented (for rule painting)
- [ ] column-rule-style — implemented (for rule painting)
- [ ] column-rule-width — implemented (for rule painting)
- [ ] column-span — partial — Parsed but NOT respected
- [ ] column-width — partial — Parsed but no text flow
- [ ] columns — partial — Underlying layout not functional
- [ ] contain — partial — Paint containment works; NOT layout/size/style containment
- [ ] contain-intrinsic-block-size — missing
- [ ] contain-intrinsic-height — partial — Parsed but NOT used in sizing
- [ ] contain-intrinsic-inline-size — missing
- [ ] contain-intrinsic-size — partial — Parsed but NOT used
- [ ] contain-intrinsic-width — partial — Parsed but NOT used
- [ ] container — partial — Parsed but NO container query evaluation
- [ ] container-name — partial — Parsed but NOT referenced for query matching
- [ ] container-type — partial — Parsed but NO query engine
- [ ] content — partial — Complex content() functions only partially supported
- [x] content-visibility — implemented
- [x] counter-increment — implemented
- [x] counter-reset — implemented
- [x] counter-set — implemented
- [x] cursor — implemented
- [ ] direction — partial — Parsed but NO RTL text reordering or bidi algorithm
- [x] display — implemented (block/inline/flex/grid/table/none/flow-root/contents)

### E-F Properties

- [ ] empty-cells — partial — Parsed but NEVER used in rendering
- [x] filter — implemented (all 9 types + drop-shadow)
- [x] flex — implemented
- [x] flex-basis — implemented
- [x] flex-direction — implemented
- [x] flex-flow — implemented
- [x] flex-grow — implemented
- [x] flex-shrink — implemented
- [x] flex-wrap — implemented
- [x] float — implemented
- [x] font — implemented
- [x] font-family — implemented
- [ ] font-feature-settings — partial — Parsed, passed through, NOT applied by text renderer
- [ ] font-kerning — partial — Parsed but NOT applied
- [ ] font-language-override — partial — Parsed, no HarfBuzz language setting
- [ ] font-optical-sizing — partial — Parsed but NOT applied
- [x] font-size — implemented
- [ ] font-size-adjust — partial — Parsed but NO aspect ratio normalization
- [ ] font-stretch — partial — Parsed but NO font selection
- [x] font-style — implemented
- [ ] font-synthesis — partial — Parsed but NO fake bold/italic
- [ ] font-variant — partial — Only small-caps; no rendering
- [ ] font-variant-alternates — partial — Parsed, no glyph selection
- [ ] font-variant-caps — partial — Parsed, no rendering
- [ ] font-variant-east-asian — partial — Parsed, no rendering
- [ ] font-variant-ligatures — partial — Parsed, no ligature control
- [ ] font-variant-numeric — partial — Parsed, no rendering
- [ ] font-variant-position — partial — Parsed, no positioning
- [ ] font-variation-settings — partial — Parsed, NOT applied
- [x] font-weight — implemented
- [ ] forced-color-adjust — partial — Parsed, NEVER used

### G-L Properties

- [x] gap — implemented
- [x] grid — implemented
- [x] grid-area — implemented
- [x] grid-auto-columns — implemented
- [x] grid-auto-flow — implemented
- [x] grid-auto-rows — implemented
- [x] grid-column — implemented
- [x] grid-column-end — implemented
- [x] grid-column-start — implemented
- [x] grid-gap — implemented
- [x] grid-row — implemented
- [x] grid-row-end — implemented
- [x] grid-row-start — implemented
- [x] grid-template — implemented
- [x] grid-template-areas — implemented
- [x] grid-template-columns — implemented
- [x] grid-template-rows — implemented
- [ ] hanging-punctuation — partial — Parsed but NEVER applied in layout
- [x] height — implemented
- [ ] hyphens — partial — Parsed but NO hyphenation engine
- [ ] image-orientation — partial — Only flip for value 2
- [ ] image-rendering — partial — Parsed but ignored when drawing images
- [ ] image-resolution — missing
- [ ] ime-mode — missing
- [x] inline-size — implemented (maps to width)
- [x] inset — implemented
- [x] inset-block — implemented
- [x] inset-block-end — implemented
- [x] inset-block-start — implemented
- [x] inset-inline — implemented
- [x] inset-inline-end — implemented
- [x] inset-inline-start — implemented
- [ ] isolation — partial — Parsed but NO stacking context enforcement
- [x] justify-content — implemented
- [x] justify-items — implemented
- [x] justify-self — implemented
- [x] left — implemented
- [x] letter-spacing — implemented
- [ ] line-break — partial — Parsed but text wrapping doesn't respect values
- [x] line-height — implemented
- [x] list-style — implemented
- [x] list-style-image — implemented
- [x] list-style-position — implemented
- [x] list-style-type — implemented

### M-O Properties

- [x] margin (all sides + logical) — implemented
- [ ] mask — partial — Stored as raw string, not decomposed
- [ ] mask-border — partial — Stored, not rendered
- [ ] mask-clip — partial — Parsed but not fully applied
- [ ] mask-composite — partial — Parsed but NOT rendered
- [ ] mask-image — partial — Linear-gradient masking only
- [ ] mask-mode — partial — Parsed but NOT applied
- [ ] mask-origin — partial — Parsed, used for positioning
- [ ] mask-position — partial — Stored as raw string, not resolved
- [ ] mask-repeat — partial — Parsed, used for tiling
- [ ] mask-size — partial — Parsed, used for scaling
- [ ] mask-type — missing
- [x] max-block-size — implemented
- [x] max-height — implemented
- [x] max-inline-size — implemented
- [x] max-width — implemented
- [x] min-block-size — implemented
- [x] min-height — implemented
- [x] min-inline-size — implemented
- [x] min-width — implemented
- [ ] mix-blend-mode — partial — 11 modes work; missing hue/saturation/color/luminosity
- [x] object-fit — implemented
- [x] object-position — implemented
- [ ] offset — partial — Stored but NOT used for motion path
- [ ] offset-anchor — partial — Stored but NOT used
- [ ] offset-distance — partial — Parsed but NOT used
- [ ] offset-path — partial — Stored but NOT evaluated
- [ ] offset-position — partial — Stored but NOT used
- [ ] offset-rotate — partial — Stored but NOT used
- [x] opacity — implemented
- [x] order — implemented
- [x] orphans — implemented (multi-column only, not paged)
- [x] outline — implemented
- [x] outline-color — implemented
- [ ] outline-offset — partial — Parsed but NOT used in rendering
- [x] outline-style — implemented
- [x] outline-width — implemented
- [x] overflow — implemented
- [ ] overflow-anchor — partial — Parsed but NOT used for scroll restoration
- [ ] overflow-block — partial — Parsed but NOT applied
- [ ] overflow-clip-margin — partial — Stored, NOT used
- [ ] overflow-inline — partial — Parsed but NOT applied
- [x] overflow-wrap — implemented
- [x] overflow-x — implemented
- [x] overflow-y — implemented
- [ ] overscroll-behavior — partial — Parsed but NOT used
- [ ] overscroll-behavior-block — missing
- [ ] overscroll-behavior-inline — missing
- [ ] overscroll-behavior-x — partial — Parsed but NOT used
- [ ] overscroll-behavior-y — partial — Parsed but NOT used

### P-R Properties

- [x] padding (all sides + logical) — implemented
- [ ] page-break-after — partial — Parsed but NO pagination
- [ ] page-break-before — partial — Parsed but NO pagination
- [ ] page-break-inside — partial — Parsed but NO pagination
- [x] paint-order — implemented
- [x] perspective — implemented
- [x] perspective-origin — implemented
- [x] place-content — implemented
- [x] place-items — implemented
- [x] place-self — implemented
- [x] pointer-events — implemented
- [x] position — implemented
- [ ] print-color-adjust — partial — Parsed but NO print mode
- [ ] quotes — partial — Parsed but NOT consumed for content generation
- [x] resize — implemented
- [x] right — implemented
- [x] rotate — implemented
- [x] row-gap — implemented
- [x] ruby-position — implemented

### S-T Properties

- [x] scale — implemented
- [x] scroll-behavior — implemented
- [x] scroll-margin — implemented
- [ ] scroll-margin-block — missing (logical shorthand)
- [x] scroll-margin-block-end — implemented
- [x] scroll-margin-block-start — implemented
- [x] scroll-margin-bottom — implemented
- [ ] scroll-margin-inline — missing (logical shorthand)
- [x] scroll-margin-inline-end — implemented
- [x] scroll-margin-inline-start — implemented
- [x] scroll-margin-left — implemented
- [x] scroll-margin-right — implemented
- [x] scroll-margin-top — implemented
- [x] scroll-padding — implemented
- [x] scroll-padding-block — implemented
- [ ] scroll-padding-block-end — missing
- [ ] scroll-padding-block-start — missing
- [x] scroll-padding-bottom — implemented
- [x] scroll-padding-inline — implemented
- [ ] scroll-padding-inline-end — missing
- [ ] scroll-padding-inline-start — missing
- [x] scroll-padding-left — implemented
- [x] scroll-padding-right — implemented
- [x] scroll-padding-top — implemented
- [x] scroll-snap-align — implemented
- [x] scroll-snap-stop — implemented
- [x] scroll-snap-type — implemented
- [x] scrollbar-color — implemented
- [x] scrollbar-gutter — implemented
- [x] scrollbar-width — implemented
- [x] shape-image-threshold — implemented
- [x] shape-margin — implemented
- [x] shape-outside — implemented
- [x] tab-size — implemented
- [x] table-layout — implemented
- [x] text-align — implemented
- [x] text-align-last — implemented
- [x] text-combine-upright — implemented
- [x] text-decoration — implemented (all styles: solid/dashed/dotted/wavy/double)
- [x] text-decoration-color — implemented
- [x] text-decoration-line — implemented
- [ ] text-decoration-skip-ink — partial — Parsed but NEVER consumed in rendering
- [x] text-decoration-style — implemented
- [x] text-decoration-thickness — implemented
- [x] text-emphasis — implemented
- [x] text-emphasis-color — implemented
- [ ] text-emphasis-position — partial — Parsed but NEVER used (position hardcoded above)
- [x] text-emphasis-style — implemented
- [x] text-indent — implemented
- [x] text-justify — implemented
- [x] text-orientation — implemented (parsing; rendering incomplete)
- [x] text-overflow — implemented (clip + ellipsis)
- [x] text-rendering — implemented
- [x] text-shadow — implemented (multiple shadows + blur)
- [x] text-transform — implemented
- [ ] text-underline-offset — partial — Parsed but NOT used in decoration rendering
- [ ] text-underline-position — partial — Parsed but NOT used
- [x] top — implemented
- [x] touch-action — implemented
- [x] transform — implemented (translate/rotate/scale/skew/matrix)
- [x] transform-box — implemented
- [x] transform-origin — implemented
- [x] transform-style — implemented
- [x] transition — implemented (full animation support)
- [x] transition-delay — implemented
- [x] transition-duration — implemented
- [x] transition-property — implemented
- [x] transition-timing-function — implemented
- [x] translate — implemented

### U-Z Properties

- [ ] unicode-bidi — partial — Parsed but NO bidi algorithm
- [ ] user-select — partial — Parsed but NO selection API
- [ ] vertical-align — partial — Inline/table works; length/percentage not rendered
- [x] visibility — implemented (visible/hidden/collapse)
- [x] white-space — implemented (all 6 modes)
- [ ] widows — partial — Only multi-column, not paged media
- [x] width — implemented
- [ ] will-change — partial — Only stacking context detection, no optimization
- [ ] word-break — partial — break-all works; keep-all partial (no CJK)
- [x] word-spacing — implemented
- [x] word-wrap — implemented (alias for overflow-wrap)
- [ ] writing-mode — partial — Parsed; text rendered vertically but layout not rotated
- [x] z-index — implemented (stacking contexts + z-order sorting)

---

## CSS Selectors

### Combinators
- [x] Descendant (space)
- [x] Child (>)
- [x] Adjacent sibling (+)
- [x] General sibling (~)
- [ ] Column (||) — missing

### Pseudo-Classes — Implemented (31)
- [x] :first-child, :last-child, :only-child, :empty, :root, :scope
- [x] :first-of-type, :last-of-type, :only-of-type
- [x] :nth-child(), :nth-last-child(), :nth-of-type(), :nth-last-of-type()
- [x] :not(), :is(), :where(), :has()
- [x] :enabled, :disabled, :checked, :required, :optional
- [x] :read-only, :read-write, :default, :valid, :invalid
- [x] :in-range, :out-of-range, :placeholder-shown
- [x] :link, :any-link, :defined, :lang()

### Pseudo-Classes — Partial/Stub
- [ ] :focus — stub (data attribute only, no real focus state)
- [ ] :focus-visible — stub (lumped with :focus)
- [ ] :focus-within — stub (no real focus state)
- [ ] :hover — stub (data attribute only, no real hover tracking)
- [ ] :active — missing (always returns false)
- [ ] :visited — privacy-preserving fallback (treats as :link)
- [ ] :target — always false (needs URL fragment context)
- [ ] :autofill — always false
- [ ] :indeterminate — always false

### Pseudo-Classes — Missing (20)
- [ ] :current(), :future, :past
- [ ] :paused, :playing, :picture-in-picture, :modal, :fullscreen
- [ ] :local-link, :user-invalid, :blank
- [ ] :dir(ltr|rtl)
- [ ] :nth-col(), :nth-last-col()
- [ ] :host, :host(), :host-context()
- [ ] :target-within
- [ ] :left, :right, :first (pagination)

### Pseudo-Elements
- [ ] ::before — parsed but NOT matched by selector matcher
- [ ] ::after — parsed but NOT matched by selector matcher
- [ ] ::first-letter — missing
- [ ] ::first-line — missing
- [ ] ::selection — missing
- [ ] ::placeholder — parsed only
- [ ] ::marker — missing
- [ ] ::backdrop, ::cue, ::cue-region — missing
- [ ] ::file-selector-button — missing
- [ ] ::grammar-error, ::spelling-error — missing
- [ ] ::slotted(), ::part() — missing (shadow DOM)

### Attribute Selectors
- [x] [attr], [attr=val], [attr~=val], [attr|=val], [attr^=val], [attr$=val], [attr*=val]
- [ ] [attr=val i] (case-insensitive) — missing
- [ ] [attr=val s] (case-sensitive) — missing

---

## CSS At-Rules & Functions

### At-Rules — Implemented
- [x] @import — recursive fetch + media conditions
- [x] @media — full parser with and/or/not
- [x] @keyframes — keyframe selectors + declarations
- [x] @font-face — family, src, weight, style, unicode-range, display
- [x] @supports — feature detection with and/or/not
- [x] @layer — named/anonymous layers with cascade ordering
- [x] @property — syntax, inherits, initial-value
- [x] @scope — scope-start parsed; scope-end boundary not enforced
- [x] @container — name + condition evaluated post-layout

### At-Rules — Parsed but Not Applied
- [ ] @charset — parsed, skipped
- [ ] @namespace — parsed, skipped
- [ ] @page — parsed, skipped (no print CSS)
- [ ] @counter-style — parsed but evaluation NOT implemented
- [ ] @starting-style — recognized, content skipped
- [ ] @font-palette-values — recognized, content skipped

### At-Rules — Missing
- [ ] @color-profile
- [ ] @font-feature-values
- [ ] @view-transition

### @media Features — Implemented (10)
- [x] width, height, orientation, prefers-color-scheme, display-mode
- [x] color-gamut, min-resolution, color, hover, pointer, grid

### @media Features — Missing (10+)
- [ ] aspect-ratio, resolution (value comparison), prefers-contrast
- [ ] color-index, forced-colors, inverted-colors, monochrome
- [ ] overflow-block, overflow-inline, update, any-hover, any-pointer, scripting

### @font-face Descriptors — Missing
- [ ] font-stretch, font-feature-settings, font-variation-settings
- [ ] ascent-override, descent-override, line-gap-override, size-adjust

### CSS Functions — Implemented
- [x] calc(), min(), max(), clamp(), abs(), sign(), mod(), rem(), round()
- [x] sin(), cos(), tan(), asin(), acos(), atan(), atan2()
- [x] sqrt(), exp(), log(), pow(), hypot()
- [x] var() — full custom properties
- [x] env() — resolves to 0px fallback
- [x] linear-gradient(), radial-gradient(), conic-gradient() + repeating variants
- [x] rgb/rgba/hsl/hsla/hwb/lab/lch/oklch/oklab/color/color-mix/light-dark

### CSS Functions — Missing
- [ ] attr() — no attribute reference
- [ ] image-set(), cross-fade(), element(), paint()
- [ ] counter(), counters(), symbols() — counter display broken

---

## CSS Values & Units

### Length Units — Implemented (13/27)
- [x] px, em, rem, %, vw, vh, vmin, vmax, ch, lh
- [x] dvw/svw/lvw aliases, dvh/svh/lvh aliases

### Length Units — Missing (14)
- [ ] ex, cm, mm, in, pt, pc, Q, cap, ic, rlh, vi, vb
- [ ] cqw, cqh, cqi, cqb — mapped to vw/vh (container queries not functional)

### Angle Units — All 4 Implemented
- [x] deg, rad, grad, turn

### Missing Unit Types
- [ ] Time units (s, ms)
- [ ] Frequency units (Hz, kHz)
- [ ] Resolution units (dpi, dpcm, dppx)

### Color Formats
- [x] Hex (#RGB, #RGBA, #RRGGBB, #RRGGBBAA)
- [x] rgb()/rgba()/hsl()/hsla()/hwb()
- [x] lab()/lch()/oklch()/oklab()
- [x] color()/color-mix()/light-dark()
- [x] Relative color syntax (from <color>)
- [x] currentColor, transparent
- [ ] Named colors — only 21 of 148 (missing ~127 X11 colors)
- [ ] System colors — missing
- [ ] revert-layer — missing

---

## CSS Layout Algorithms

### Block Formatting Context
- [x] Margin collapsing (adjacent siblings, parent-child, empty blocks, negative margins)
- [x] Anonymous block boxes
- [x] Clearance computation
- [x] Float interaction with BFC

### Inline Formatting Context
- [x] Line box construction, baseline alignment, vertical-align (9 values)
- [x] Text wrapping, white-space processing (6 modes)
- [ ] Bidi algorithm — MISSING (no RTL text reordering)
- [ ] Ruby layout — MISSING (fields exist, no implementation)
- [ ] First-line/first-letter special handling — MISSING

### Float Layout
- [x] Float positioning (left, right), clearing, BFC avoidance
- [x] Float intrusion into line boxes, shape-outside support

### Flexbox
- [x] Main/cross axis, sizing algorithm (basis/grow/shrink), multi-line wrap
- [x] Alignment (justify-content, align-items/self/content), order, auto margins
- [ ] Intrinsic size contribution of flex items — missing
- [ ] Visibility:collapse in flex — missing

### Grid
- [x] Explicit/implicit grid, placement algorithm, track sizing
- [x] Named lines and grid areas, alignment, gap
- [ ] Subgrid — missing
- [ ] Masonry — missing

### Table
- [x] Full box model, column width algorithm, colspan/rowspan
- [x] Border collapsing, cell alignment, caption positioning
- [ ] Empty cells handling — missing

### Multi-Column — ALL MISSING
- [ ] Column count/width/gap/rule/fill/span/breaking — fields exist, zero implementation

### Positioning
- [x] Static, relative, absolute, fixed, sticky
- [ ] Stacking contexts — z-index parsed but children NOT sorted during paint (CRITICAL)
- [ ] Transform/filter/opacity-based containing blocks — missing

### Sizing
- [x] Content-box vs border-box, min/max constraints
- [x] Intrinsic sizing (min-content, max-content, fit-content)
- [x] Percentage resolution, aspect ratio
- [ ] Writing modes effect on sizing — missing

---

## CSS Paint & Visual Rendering

### Backgrounds — Mostly Implemented
- [x] background-color, background-image (url + gradients), background-position
- [x] background-size (cover/contain/explicit), background-repeat (space/round)
- [x] background-clip, background-origin, background-attachment
- [ ] background-blend-mode — partial (6 of 16 modes)
- [ ] background-clip: text — NOT implemented
- [ ] Multiple backgrounds — limited to one gradient + one image per element

### Borders
- [x] Solid/dashed/dotted + double/groove/ridge/inset/outset for outlines
- [x] Border-radius (circular only)
- [ ] Elliptical border-radius — missing
- [ ] Border-image 9-slice for URL sources — missing (gradient only)
- [x] Border-collapse, border-spacing

### Box Effects
- [x] Box-shadow (multiple, inset, spread, blur), opacity, visibility
- [x] Filter (9 types + drop-shadow), clip-path (4 shapes)
- [ ] Backdrop-filter — stub (no pixel manipulation)
- [ ] Mask properties — all parsed but NEVER rendered
- [ ] Isolation — parsed but no stacking context enforcement

### Text Rendering
- [x] Font selection/fallback, weight, size, style (not oblique angle)
- [x] Letter-spacing, word-spacing, text-decoration (5 styles), text-shadow
- [x] Text-transform, text-indent, text-overflow (clip/ellipsis)
- [x] White-space (6 modes), text-emphasis marks
- [ ] Text-align: justify — NOT actually rendered (no word spacing adjustment)
- [ ] Hyphens — no auto-hyphenation
- [ ] Writing-mode — vertical rendering only, no layout rotation
- [ ] Direction/unicode-bidi — no RTL reordering
- [ ] Font-variant-* — all parsed, none rendered
- [ ] ::first-line, ::first-letter, ::selection — NOT implemented

### Gradients — All 6 Implemented
- [x] linear-gradient, radial-gradient, conic-gradient + repeating variants

### Transforms
- [x] translate, scale, skew, matrix (2D)
- [ ] 3D transforms — NOT supported (no matrix3d, perspective foreshortening, preserve-3d)
- [ ] rotate3d/rotateX/rotateY — NOT supported

### Stacking/Painting
- [x] Stacking context creation rules, overflow clipping
- [x] Z-index sorting (stacking_negative/zero/positive arrays)
- [ ] Scrollbar interactive dragging — missing

---

## HTML Elements

### Fully Working (130+)
All standard HTML5 elements are parsed and have at least basic rendering:
- Document structure (html, head, body, meta, link, title, style, script)
- Sections (article, section, nav, aside, header, footer, main, h1-h6)
- Grouping (p, div, pre, blockquote, ol, ul, li, dl, dt, dd, figure, hr)
- Text (a, em, strong, code, span, br, sub, sup, mark, etc.)
- Tables (table, thead, tbody, tfoot, tr, th, td, caption, colgroup, col)
- Forms (form, input [all types], button, select, textarea, label, fieldset, legend)
- Interactive (details/summary toggle, dialog)
- Embedded (img, picture, canvas, svg, video/audio placeholders, iframe placeholder)
- SVG (rect, circle, ellipse, line, path, text, polygon, polyline, gradients)

### Placeholder Only
- [ ] video — renders black box with play button, no playback
- [ ] audio — renders gray bar with time display, no playback
- [ ] iframe — renders gray placeholder, no nested browsing

---

## HTML Parsing

### Tokenizer
- [x] Named entities — 585/2231 (missing ~1646 rare entities)
- [x] Numeric entities (decimal + hex)
- [x] Comments, DOCTYPE, script/RCDATA states, attributes
- [ ] Processing instructions — treated as bogus comment

### Tree Construction
- [x] Adoption agency algorithm (full)
- [x] Active formatting elements + reconstruction
- [x] Scope checking (in-scope, button, list-item, select)
- [x] Implicit element creation (html, head, body)
- [x] 12/14 insertion modes implemented
- [ ] Foster parenting — MISSING
- [ ] InSelect mode — NOT implemented
- [ ] InTemplate mode — NOT implemented
- [ ] Table scope — missing
- [ ] Foreign content (SVG/MathML) — no namespace switching
- [ ] Duplicate attributes — NO deduplication (broken)
- [ ] Form association algorithm — missing

### Encoding
- [x] UTF-8
- [ ] Encoding detection — missing (no charset/BOM scanning)
- [ ] ISO-8859-1, Windows-1252 — NOT supported

---

## JS DOM Core API

### Implemented
- [x] Node: nodeType, nodeName, textContent, parentNode, childNodes, firstChild, lastChild, siblings, insertBefore, appendChild, replaceChild, removeChild, cloneNode, contains, etc.
- [x] Element: tagName, id, className, classList, innerHTML, outerHTML, getAttribute/setAttribute, querySelector/querySelectorAll, closest, matches, getBoundingClientRect, etc.
- [x] Document: documentElement, head, body, title, cookie, createElement, getElementById, getElementsByClassName, getElementsByTagName, querySelector/querySelectorAll, etc.

### Missing/Partial
- [ ] Node.nodeValue — missing on regular nodes
- [ ] Node.ownerDocument — missing as actual property
- [ ] Element.attributes (NamedNodeMap) — missing
- [ ] Element.getAttributeNode/setAttributeNode/removeAttributeNode — missing
- [ ] Element.getElementsByClassName/TagName (scoped) — missing (only on document)
- [ ] Element.getClientRects() — stub (returns empty array)
- [ ] Element.scrollBy() — missing
- [ ] Element.computedStyleMap() — missing
- [ ] Document.URL — missing as property
- [ ] Document.baseURI — missing
- [ ] Document.characterSet/charset — missing
- [ ] Document.doctype — not exposed to JS
- [ ] Document.compatMode — missing
- [ ] Document.open/close — missing
- [ ] Document.execCommand — stub (always false)
- [ ] Text.splitText() — missing

---

## JS Events API

### Implemented
- [x] Event base (constructor, type, target, phases, bubbles, cancelable, propagation)
- [x] addEventListener with options (capture, once, passive, signal)
- [x] removeEventListener, dispatchEvent
- [x] Mouse events (all properties: clientX/Y, pageX/Y, button, modifiers)
- [x] Keyboard event constructor (key, code, keyCode, location, repeat)
- [x] 16 event constructors registered globally
- [x] MutationObserver, IntersectionObserver, ResizeObserver
- [x] AbortController/AbortSignal (full)
- [x] CustomEvent with detail

### Actually Dispatched Events (Only 14!)
- [x] click, mousedown, mouseup, mousemove, mouseover, mouseout
- [x] mouseenter, mouseleave
- [x] focus, blur, focusin, focusout
- [x] input, change
- [x] DOMContentLoaded

### CRITICAL: Events Never Dispatched
- [ ] Keyboard events (keydown, keyup, keypress) — NO keyboard input handler
- [ ] Form events (submit, reset) — never fire
- [ ] Window load/beforeunload/unload — only DOMContentLoaded
- [ ] Scroll event — not dispatched on viewport scroll
- [ ] Resize event — not dispatched on window resize
- [ ] dblclick — never dispatched
- [ ] contextmenu — never dispatched
- [ ] Animation/transition events — constructors exist, no dispatch
- [ ] Pointer events — constructors exist, MouseEvent used instead
- [ ] Touch events — constructors exist, no touch support
- [ ] Drag events — constructors exist, no drag engine
- [ ] Hash/popstate — constructors exist, no navigation dispatch
- [ ] Storage, message, visibility, selection events — all missing

---

## JS Web APIs

### Fully Working
- [x] Fetch API (fetch, Request, Response, Headers, AbortController)
- [x] URL/URLSearchParams (full WHATWG spec)
- [x] localStorage/sessionStorage
- [x] setTimeout/setInterval/requestAnimationFrame
- [x] Console (log, warn, error, info, debug, trace, assert, time/timeEnd, count)
- [x] TextEncoder/TextDecoder, atob/btoa
- [x] FormData (all methods)
- [x] Blob/File/FileReader
- [x] History (pushState, replaceState, back, forward, go)
- [x] Location (all properties + assign/replace)
- [x] crypto.getRandomValues/randomUUID
- [x] Performance (now, mark, measure, getEntries)
- [x] Worker constructor + postMessage
- [x] matchMedia (full media query evaluation)
- [x] getComputedStyle
- [x] structuredClone (JSON round-trip)
- [x] queueMicrotask
- [x] WebSocket (full RFC 6455)
- [x] XMLHttpRequest

### Stubs/Missing
- [ ] crypto.subtle (SubtleCrypto) — stub
- [ ] IndexedDB — missing
- [ ] Service Worker — fake registration only
- [ ] postMessage (cross-origin) — missing
- [ ] window.print/focus/blur — missing/no-op
- [ ] resizeTo/resizeBy/moveTo/moveBy — missing
- [ ] history.scrollRestoration — missing
- [ ] location.reload() — no-op
- [ ] navigator.mediaDevices — empty stub
- [ ] navigator.geolocation — stub (empty results)
- [ ] navigator.credentials — missing
- [ ] console.countReset, timeLog, dirxml, profile — missing
- [ ] FileList — missing
- [ ] OffscreenCanvas — missing
- [ ] SharedWorker — stub (errors on instantiation)

---

## JS Language Features

### QuickJS provides near-complete ES2024 support:
- [x] let/const, arrow functions, template literals, destructuring, spread/rest
- [x] Classes (private #fields, static blocks), modules (import/export, dynamic import)
- [x] Promises (all/allSettled/any/race/try/withResolvers), async/await, top-level await
- [x] Generators, iterators, for...of
- [x] Map/Set/WeakMap/WeakSet/WeakRef/FinalizationRegistry
- [x] Symbol, Proxy/Reflect, BigInt, globalThis
- [x] Optional chaining (?.), nullish coalescing (??)
- [x] Logical assignment (&&=, ||=, ??=), numeric separators
- [x] RegExp (named groups, lookbehind, dotAll, unicode, v flag, modifiers)
- [x] Object.groupBy, Array.at/findLast/toReversed/toSorted/toSpliced
- [x] String.isWellFormed/toWellFormed, matchAll, replaceAll
- [x] Atomics API, Error cause

### Missing
- [ ] Intl API (DateTimeFormat, NumberFormat, Collator, etc.) — QuickJS limitation
- [ ] Temporal API — not yet standardized in QuickJS

---

## Networking & Protocols

### HTTP
- [x] HTTP/1.1 — full support (default)
- [x] HTTP/2 — full RFC 7540 with HPACK
- [x] HTTPS/TLS — SecureTransport (macOS native)
- [x] All methods (GET/POST/PUT/DELETE/PATCH/HEAD/OPTIONS)
- [x] Chunked transfer encoding
- [x] Gzip/deflate decompression
- [x] Keep-alive connection pooling (6/host, 30 total, 60s timeout)
- [x] HTTP cache (LRU, 50MB, ETag/If-None-Match/304)
- [x] Cookies (full RFC 6265 + SameSite)
- [x] Redirects (301/302/303/307/308 with method preservation)
- [x] CORS policy enforcement

### Missing/Broken
- [ ] HTTP/3 / QUIC — missing
- [ ] Brotli decompression — ADVERTISED BUT NOT DECOMPRESSED (silent failure bug!)
- [ ] HTTPS connection pooling — connections closed after each request
- [ ] 1xx informational responses — not handled
- [ ] Content-Security-Policy — not implemented
- [ ] HSTS — not implemented
- [ ] Referrer-Policy — not implemented (Referer header not sent)
- [ ] Resource hints (preload, prefetch, dns-prefetch, preconnect) — missing
- [ ] Lazy loading — missing
- [ ] Async/defer script loading — all scripts synchronous
- [ ] Service Worker — stub only
- [ ] Authorization header helpers — missing

---

## Image & Media Support

### Image Formats
- [x] JPEG, PNG (with alpha), BMP, WebP, SVG
- [x] GIF (static only)
- [ ] GIF (animated) — no frame sequencing
- [ ] AVIF — no decoder
- [ ] TIFF — macOS native only

### Image Features
- [x] Object-fit/object-position, aspect ratio, natural dimensions
- [x] Alt text fallback for broken images
- [ ] srcset/sizes — parsed but NOT evaluated (no DPI selection)
- [ ] loading="lazy" — missing
- [ ] Image decode() API — missing

### SVG
- [x] Basic shapes, text, gradients, transforms, use/defs
- [ ] Patterns, clipPath, mask, filters — missing
- [ ] SMIL animations — missing
- [ ] preserveAspectRatio — missing

### Canvas 2D — Fully Implemented
- [x] All drawing, path, style, transform, gradient, text, state, pixel APIs
- [x] toDataURL/toBlob
- [ ] OffscreenCanvas — missing

### Video/Audio — Placeholder Only
- [ ] No actual media playback (renders static UI placeholder)
- [ ] No HTMLMediaElement API (play/pause/currentTime/duration)
- [ ] No media events

### Iframe — Placeholder Only
- [ ] No nested browsing context, sandboxing, srcdoc, postMessage

---

## Statistics Summary

| Category | Implemented | Partial | Missing | Total |
|----------|------------|---------|---------|-------|
| CSS Properties (~350) | ~160 | ~130 | ~60 | ~350 |
| CSS Selectors (~100) | ~40 | ~10 | ~50 | ~100 |
| CSS At-Rules (18) | 9 | 6 | 3 | 18 |
| CSS Functions (~40) | ~30 | 0 | ~10 | ~40 |
| CSS Units (~40) | 17 | 0 | ~23 | ~40 |
| CSS Layout Features (~80) | ~55 | ~10 | ~15 | ~80 |
| CSS Paint Features (~130) | ~85 | ~28 | ~17 | ~130 |
| HTML Elements (~150) | ~130 | ~15 | ~5 | ~150 |
| HTML Parsing (~30) | ~15 | ~8 | ~7 | ~30 |
| JS DOM APIs (~100) | ~70 | ~15 | ~15 | ~100 |
| JS Events (~70) | ~30 | ~5 | ~35 | ~70 |
| JS Web APIs (~150) | ~95 | ~18 | ~37 | ~150 |
| JS Language (~80) | ~78 | 0 | ~2 | ~80 |
| Networking (~60) | ~35 | ~5 | ~20 | ~60 |
| Image/Media (~50) | ~25 | ~5 | ~20 | ~50 |
| **TOTAL** | **~875** | **~255** | **~319** | **~1449** |
