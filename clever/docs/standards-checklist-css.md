# CSS Properties Standards Checklist

> Clever Browser Engine -- CSS property implementation status.
> Generated 2026-02-25 from `style_resolver.cpp` and `computed_style.h`.
>
> Legend:
> - `[x]` -- Fully parsed and stored in ComputedStyle
> - `[~]` -- Partial (notes on what is missing)
> - `[ ]` -- Not implemented

---

## 1. Box Model

### Sizing

- [x] `width` -- supports px, em, rem, %, vw, vh, calc(), auto
- [x] `height` -- same as width
- [x] `min-width`
- [x] `max-width`
- [x] `min-height`
- [x] `max-height`
- [x] `box-sizing` -- content-box, border-box
- [x] `aspect-ratio` -- auto, ratio (e.g. 16/9), single number

### Logical Sizing

- [x] `inline-size` -- maps to width (horizontal-tb)
- [x] `block-size` -- maps to height (horizontal-tb)
- [x] `min-inline-size`
- [x] `max-inline-size`
- [x] `min-block-size`
- [x] `max-block-size`

### Margin

- [x] `margin` -- 1-4 value shorthand, auto
- [x] `margin-top`
- [x] `margin-right`
- [x] `margin-bottom`
- [x] `margin-left`
- [x] `margin-block` -- logical shorthand (1-2 values)
- [x] `margin-inline` -- logical shorthand (1-2 values)
- [x] `margin-block-start` -- maps to margin-top (Cycle 236)
- [x] `margin-block-end` -- maps to margin-bottom (Cycle 236)
- [x] `margin-inline-start` -- maps to margin-left (Cycle 236)
- [x] `margin-inline-end` -- maps to margin-right (Cycle 236)
- [x] `margin-trim` -- none/block/inline/block-start/block-end/inline-start/inline-end (Cycle 252)

### Padding

- [x] `padding` -- 1-4 value shorthand
- [x] `padding-top`
- [x] `padding-right`
- [x] `padding-bottom`
- [x] `padding-left`
- [x] `padding-block` -- logical shorthand (1-2 values)
- [x] `padding-inline` -- logical shorthand (1-2 values)
- [x] `padding-block-start` -- maps to padding-top (Cycle 236)
- [x] `padding-block-end` -- maps to padding-bottom (Cycle 236)
- [x] `padding-inline-start` -- maps to padding-left (Cycle 236)
- [x] `padding-inline-end` -- maps to padding-right (Cycle 236)

---

## 2. Border

### Border Shorthand

- [x] `border` -- width + style + color shorthand
- [x] `border-top` / `border-right` / `border-bottom` / `border-left` -- via `border` shorthand application
- [x] `border-color` -- 1-4 value shorthand
- [x] `border-width` -- 1-4 value shorthand
- [x] `border-style` -- 1-4 value shorthand

### Border Individual Properties

- [x] `border-top-width` / `border-right-width` / `border-bottom-width` / `border-left-width`
- [x] `border-top-style` / `border-right-style` / `border-bottom-style` / `border-left-style`
- [x] `border-top-color` / `border-right-color` / `border-bottom-color` / `border-left-color`

### Border Style Values

- [x] `none`, `solid`, `dashed`, `dotted`, `double`, `groove`, `ridge`, `inset`, `outset`
- [ ] `hidden` -- NOT IMPLEMENTED

### Border Radius

- [x] `border-radius` -- 1-4 value shorthand
- [x] `border-top-left-radius`
- [x] `border-top-right-radius`
- [x] `border-bottom-left-radius`
- [x] `border-bottom-right-radius`
- [x] Elliptical radii (e.g. `10px / 5px`) -- Full parsing, per-corner h/v averaged (Cycle 242)

### Border Logical Properties

- [x] `border-block` -- shorthand
- [x] `border-block-start` / `border-block-end`
- [x] `border-inline-start` / `border-inline-end`
- [x] `border-inline-width`
- [x] `border-block-width`
- [x] `border-inline-color`
- [x] `border-block-color`
- [x] `border-inline-style`
- [x] `border-block-style`
- [x] `border-start-start-radius` / `border-start-end-radius` / `border-end-start-radius` / `border-end-end-radius`

### Border Collapse (Tables)

- [x] `border-collapse` -- separate, collapse
- [x] `border-spacing` -- 1-2 value (horizontal, vertical)

### Border Image

- [x] `border-image` -- shorthand (source, slice, repeat)
- [x] `border-image-source` -- url() and gradient support
- [x] `border-image-slice` -- percentage + fill keyword
- [x] `border-image-width`
- [x] `border-image-outset`
- [x] `border-image-repeat` -- stretch, repeat, round, space

---

## 3. Display & Layout

### Display

- [x] `display` -- block, inline, inline-block, flex, inline-flex, none, list-item, table, table-row, table-cell, grid, inline-grid, contents
- [~] `-webkit-box` / `-webkit-inline-box` -- mapped to flex (legacy)
- [~] `display: table-column` / `table-column-group` / `table-footer-group` / `table-caption` / `table-row-group` / `table-header-group` -- mapped to closest display types (Cycle 255)
- [~] `display: ruby` / `ruby-text` -- mapped to Inline (no real ruby layout) (Cycle 253)
- [x] `display: flow-root` -- mapped to Block (creates BFC) (Cycle 229)

### Position

- [x] `position` -- static, relative, absolute, fixed, sticky
- [x] `-webkit-sticky` -- mapped to sticky

### Position Offsets

- [x] `top` / `right` / `bottom` / `left`
- [x] `inset` -- 1-4 value shorthand
- [x] `inset-block` -- logical shorthand
- [x] `inset-inline` -- logical shorthand
- [x] `inset-block-start` / `inset-block-end` -- maps to top/bottom (Cycle 236)
- [x] `inset-inline-start` / `inset-inline-end` -- maps to left/right (Cycle 236)

### Stacking

- [x] `z-index` -- integer values

### Float & Clear

- [x] `float` -- left, right, none
- [x] `clear` -- left, right, both, none
- [x] `float: inline-start` / `inline-end` -- mapped to left/right (LTR) (Cycle 253)

### Overflow

- [x] `overflow` -- 1-2 value shorthand (visible, hidden, scroll, auto)
- [x] `overflow-x`
- [x] `overflow-y`
- [x] `overflow-block` -- visible, hidden, scroll, auto, clip
- [x] `overflow-inline` -- visible, hidden, scroll, auto, clip
- [x] `overflow-wrap` / `word-wrap` -- normal, break-word, anywhere
- [x] `overflow-anchor` -- auto, none
- [x] `overflow-clip-margin`

---

## 4. Flexbox

- [x] `flex-direction` -- row, row-reverse, column, column-reverse
- [x] `flex-wrap` -- nowrap, wrap, wrap-reverse
- [x] `flex-flow` -- shorthand (direction + wrap)
- [x] `flex` -- shorthand (none, auto, or grow/shrink/basis)
- [x] `flex-grow`
- [x] `flex-shrink`
- [x] `flex-basis` -- length, auto
- [x] `justify-content` -- flex-start, flex-end, center, space-between, space-around, space-evenly
- [x] `align-items` -- flex-start, flex-end, center, baseline, stretch
- [x] `align-self` -- auto, flex-start, flex-end, center, baseline, stretch
- [x] `align-content` -- start, end, center, stretch, space-between, space-around
- [x] `order`
- [x] `gap` / `grid-gap` -- 1-2 value shorthand (row-gap, column-gap)
- [x] `row-gap` / `grid-row-gap`
- [x] `column-gap` / `grid-column-gap`

### Alignment Shorthands

- [x] `justify-items` -- start, end, center, stretch
- [x] `justify-self` -- auto, start, end, center, baseline, stretch
- [x] `place-items` -- shorthand (align-items + justify-items)
- [x] `place-content` -- shorthand (align-content + justify-content)
- [x] `place-self` -- shorthand (align-self + justify-self)
- [x] `-webkit-box-orient` -- mapped to flex-direction

---

## 5. CSS Grid

- [x] `grid-template-columns` -- stored as string (parsed at layout time)
- [x] `grid-template-rows` -- stored as string
- [x] `grid-template-areas` -- stored as string
- [x] `grid-column` -- e.g. "1 / 3"
- [x] `grid-row` -- e.g. "1 / 2"
- [x] `grid-area` -- e.g. "header" or "1 / 2 / 3 / 4"
- [x] `grid-auto-rows` -- stored as string
- [x] `grid-auto-columns` -- stored as string
- [x] `grid-auto-flow` -- row/column/dense (Cycle 229)
- [x] `grid-template` -- shorthand parsing rows / columns with `/` separator (Cycle 244)
- [x] `grid` -- shorthand parsing rows / columns (same as grid-template) (Cycle 244)
- [x] `grid-column-start` / `grid-column-end` -- individual longhands (Cycle 235)
- [x] `grid-row-start` / `grid-row-end` -- individual longhands (Cycle 235)

---

## 6. Typography & Fonts

### Font Properties

- [x] `font-family` -- parsed and stored, quotes stripped
- [x] `font-size` -- px, em, rem, %, named sizes via parse_length
- [x] `font-weight` -- normal, bold, bolder, lighter, 100-900
- [x] `font-style` -- normal, italic, oblique
- [x] `font-variant` -- normal, small-caps
- [x] `font-variant-caps` -- normal, small-caps, all-small-caps, petite-caps, all-petite-caps, unicase, titling-caps
- [x] `font-variant-numeric` -- normal, ordinal, slashed-zero, lining-nums, oldstyle-nums, proportional-nums, tabular-nums
- [x] `font-variant-ligatures` -- normal, none, common-ligatures, no-common-ligatures, discretionary-ligatures, no-discretionary-ligatures
- [x] `font-variant-east-asian` -- normal, jis78, jis83, jis90, jis04, simplified, traditional, full-width, proportional-width, ruby
- [x] `font-variant-alternates` -- normal, historical-forms
- [x] `font-variant-position` -- normal, sub, super
- [x] `font-synthesis` -- bitmask: weight, style, small-caps
- [x] `font-feature-settings` -- stored as raw string
- [x] `font-variation-settings` -- stored as raw string
- [x] `font-optical-sizing` -- auto, none
- [x] `font-kerning` -- auto, normal, none
- [x] `font-size-adjust` -- none or numeric value
- [x] `font-stretch` -- ultra-condensed through ultra-expanded
- [x] `font-language-override` -- normal or quoted string
- [x] `font-palette` -- normal, light, dark, or custom string
- [x] `font-smooth` / `-webkit-font-smoothing` -- auto, none, antialiased, subpixel-antialiased
- [x] `font` -- full shorthand parser (style/variant/weight/size/line-height/family) (Cycle 229)
- [x] `font-display` -- parsed in @font-face (swap/block/fallback/optional/auto) (Cycle 205)

### Text Properties

- [x] `color`
- [x] `line-height` -- normal, unitless multiplier, px, em, %
- [x] `text-align` -- left, right, center, justify, start, end, -webkit-center/left/right
- [x] `text-align-last` -- auto, left/start, right/end, center, justify
- [x] `text-indent`
- [x] `text-transform` -- none, capitalize, uppercase, lowercase
- [x] `text-overflow` -- clip, ellipsis, fade
- [x] `text-decoration` / `text-decoration-line` -- none, underline, overline, line-through (also parses style, color, thickness inline)
- [x] `text-decoration-color`
- [x] `text-decoration-style` -- solid, dashed, dotted, wavy, double
- [x] `text-decoration-thickness`
- [x] `text-decoration-skip` -- none, objects, spaces, ink, edges, box-decoration
- [x] `text-decoration-skip-ink` -- auto, none, all
- [x] `text-underline-offset`
- [x] `text-underline-position` -- auto, under, left, right
- [x] `text-shadow` -- offset-x, offset-y, blur, color (multiple shadows stored in vector)
- [x] `text-emphasis-style` -- stored as string
- [x] `text-emphasis-color` -- ARGB
- [x] `text-emphasis` -- shorthand + text-emphasis-position (Cycle 235)
- [x] `text-rendering` -- auto, optimizeSpeed, optimizeLegibility, geometricPrecision
- [x] `text-size-adjust` / `-webkit-text-size-adjust` -- auto, none, or percentage
- [x] `text-orientation` -- mixed, upright, sideways
- [x] `text-combine-upright` -- none, all, digits
- [x] `text-justify` -- auto, inter-word, inter-character, none
- [x] `text-wrap` / `text-wrap-mode` -- wrap, nowrap, balance, pretty, stable
- [x] `text-wrap-style` -- balance, pretty, stable

### Spacing

- [x] `letter-spacing` -- normal, or length
- [x] `word-spacing` -- normal, or length
- [x] `word-break` -- normal, break-all, keep-all
- [x] `white-space` -- normal, nowrap, pre, pre-wrap, pre-line, break-spaces
- [x] `white-space-collapse` -- collapse, preserve, preserve-breaks, break-spaces
- [x] `line-break` -- auto, loose, normal, strict, anywhere
- [x] `tab-size` / `-moz-tab-size` -- integer
- [x] `hyphens` -- none, manual, auto
- [x] `hanging-punctuation` -- none, first, last, force-end, allow-end, first last

### Vertical Alignment

- [x] `vertical-align` -- baseline, top, middle, bottom, text-top, text-bottom
- [x] `vertical-align` (length/percentage values) -- px/em/rem/% alongside keywords (Cycle 235)

### Direction & Writing

- [x] `direction` -- ltr, rtl
- [x] `unicode-bidi` -- normal, embed, bidi-override, isolate, isolate-override, plaintext
- [x] `writing-mode` -- horizontal-tb, vertical-rl, vertical-lr

---

## 7. Backgrounds

- [~] `background` -- parsed for gradients and colors; url() images are handled separately; multi-layer support uses last layer only
- [x] `background-color`
- [~] `background-image` -- url() stored; linear-gradient, radial-gradient, conic-gradient parsed (including repeating variants)
- [x] `background-size` -- auto, cover, contain, explicit width/height
- [x] `background-repeat` -- repeat, repeat-x, repeat-y, no-repeat
- [x] `background-position` -- keyword (left/center/right, top/center/bottom) and pixel offset
- [x] `background-clip` / `-webkit-background-clip` -- border-box, padding-box, content-box, text
- [x] `background-origin` -- padding-box, border-box, content-box
- [x] `background-blend-mode` -- normal, multiply, screen, overlay, darken, lighten
- [x] `background-attachment` -- scroll, fixed, local
- [x] `background-position-x` / `background-position-y` -- individual longhands

### Gradients

- [x] `linear-gradient()` -- angle, direction keywords (to top/right/bottom/left), color stops
- [x] `radial-gradient()` -- circle/ellipse shape, color stops
- [x] `conic-gradient()` -- from angle, color stops
- [x] `repeating-linear-gradient()` / `repeating-radial-gradient()` / `repeating-conic-gradient()`
- [x] Gradient color stop positions -- explicit positions (e.g. `red 20%`, `blue 80%`) now parsed in cascade (Cycle 242)

---

## 8. Visual Effects

### Opacity & Visibility

- [x] `opacity` -- 0.0 to 1.0
- [x] `visibility` -- visible, hidden, collapse

### Box Shadow

- [x] `box-shadow` -- offset-x, offset-y, blur, spread, color; `none`
- [x] Multiple shadows -- comma-separated, stored in BoxShadowEntry vector, painted in reverse order
- [x] `inset` keyword -- parsed from both cascade and inline styles, rendered as inset shadow

### Filters

- [x] `filter` -- grayscale, sepia, brightness, contrast, invert, saturate, opacity, hue-rotate, blur, drop-shadow
- [x] `backdrop-filter` / `-webkit-backdrop-filter` -- grayscale, sepia, brightness, contrast, invert, saturate, opacity, hue-rotate, blur

### Blend Modes

- [x] `mix-blend-mode` -- normal, multiply, screen, overlay, darken, lighten, color-dodge, color-burn, hard-light, soft-light, difference, exclusion
- [x] `isolation` -- auto, isolate

### Clip Path

- [x] `clip-path` -- none, circle(), ellipse(), inset() (with `at` position support)
- [x] `clip-path: polygon()` -- comma-separated coordinate pairs (Cycle 235)
- [~] `clip-path: url()` (SVG reference) -- parsed and stored, type=6 (Cycle 253)
- [x] `clip-path: path()` -- SVG path data string (type=5)

### Mask

- [x] `mask-image` / `-webkit-mask-image` -- stored as string
- [x] `mask-size` / `-webkit-mask-size` -- auto, cover, contain, explicit
- [x] `mask-repeat` / `-webkit-mask-repeat` -- repeat, repeat-x, repeat-y, no-repeat, space, round
- [x] `mask-composite` / `-webkit-mask-composite` -- add, subtract, intersect, exclude
- [x] `mask-mode` -- match-source, alpha, luminance
- [x] `mask` / `-webkit-mask` -- shorthand stored as string
- [x] `mask-origin` / `-webkit-mask-origin` -- border-box (0), padding-box (1), content-box (2)
- [x] `mask-position` / `-webkit-mask-position` -- stored as string, default "0% 0%"
- [x] `mask-clip` / `-webkit-mask-clip` -- border-box (0), padding-box (1), content-box (2), no-clip (3)
- [~] `mask-border` (all longhands) -- parsed and stored as string (Cycle 253)

### Shape Outside

- [x] `shape-outside` -- none, circle(), ellipse(), inset(), margin-box, border-box, padding-box, content-box; also stored as raw string
- [x] `shape-margin`
- [x] `shape-image-threshold`
- [ ] `shape-outside: polygon()` -- NOT IMPLEMENTED
- [ ] `shape-outside: url()` -- NOT IMPLEMENTED

---

## 9. Transforms

### Transform Property

- [x] `transform` -- supports: translate(), translateX(), translateY(), rotate(), scale(), scaleX(), scaleY(), skew(), skewX(), skewY(), matrix()
- [x] `transform: translate3d()` / `rotate3d()` / `scale3d()` / `perspective()` / `matrix3d()` -- 3D→2D mapping (Cycle 236)
- [x] `transform: translateZ()` / `scaleZ()` / `rotateX()` / `rotateY()` / `rotateZ()` -- 3D no-ops/2D mapping (Cycle 236)

### Transform Sub-properties

- [x] `transform-origin` -- keywords (left/center/right, top/center/bottom) and percentage
- [x] `transform-style` -- flat, preserve-3d
- [x] `transform-box` -- content-box/border-box/fill-box/stroke-box/view-box (Cycle 245)

### Individual Transform Properties (CSS Transforms Level 2)

- [x] `rotate` -- stored as string
- [x] `scale` -- stored as string
- [x] `translate` -- stored as string

### Perspective

- [x] `perspective` -- none or length
- [x] `perspective-origin` -- keywords and percentage

---

## 10. Transitions & Animations

### Transitions

- [x] `transition` -- shorthand (comma-separated multiple transitions)
- [x] `transition-property`
- [x] `transition-duration`
- [x] `transition-timing-function` -- ease, linear, ease-in, ease-out, ease-in-out
- [x] `transition-delay`
- [x] `transition-timing-function: cubic-bezier()` -- Newton-Raphson evaluation (Cycle 230)
- [x] `transition-timing-function: steps()` -- step-start/step-end/steps(n) (Cycle 230)
- [x] `transition-behavior` -- normal (0), allow-discrete (1)

### Animations

- [x] `animation` -- shorthand (name, duration, timing, delay, count, direction, fill-mode)
- [x] `animation-name`
- [x] `animation-duration`
- [x] `animation-timing-function` -- ease, linear, ease-in, ease-out, ease-in-out
- [x] `animation-delay`
- [x] `animation-iteration-count` -- number or infinite
- [x] `animation-direction` -- normal, reverse, alternate, alternate-reverse
- [x] `animation-fill-mode` -- none, forwards, backwards, both
- [x] `animation-play-state` -- running/paused (Cycle 235)
- [x] `animation-composition` -- replace/add/accumulate (Cycle 245)
- [x] `animation-timeline` -- auto/none/custom string (Cycle 245)
- [x] `animation-range` -- stored as string (e.g. "entry 10% exit 90%")

---

## 11. List & Counters

- [x] `list-style` -- shorthand (type, position, image)
- [x] `list-style-type` -- disc, circle, square, decimal, decimal-leading-zero, lower-roman, upper-roman, lower-alpha, upper-alpha, none, lower-greek, lower-latin, upper-latin
- [x] `list-style-position` -- inside, outside
- [x] `list-style-image` -- url() (tokenizer space-stripping handled)
- [x] `counter-increment` -- stored as raw string
- [x] `counter-reset` -- stored as raw string
- [x] `counter-set` -- string value, cascade parsing (Cycle 245)

---

## 12. Table Properties

- [x] `table-layout` -- auto, fixed
- [x] `border-collapse` -- separate, collapse
- [x] `border-spacing` -- 1-2 values (horizontal, vertical)
- [x] `caption-side` -- top, bottom
- [x] `empty-cells` -- show, hide

---

## 13. Multi-Column Layout

- [x] `column-count` -- auto or integer
- [x] `column-width` -- auto or length
- [x] `columns` -- shorthand (count + width)
- [x] `column-gap` -- length
- [x] `column-rule` -- shorthand (width, style, color)
- [x] `column-rule-width`
- [x] `column-rule-color`
- [x] `column-rule-style` -- none, solid, dashed, dotted
- [x] `column-span` -- none, all
- [x] `column-fill` -- balance/auto/balance-all (Cycle 245)

---

## 14. Outline

- [x] `outline` -- shorthand (width, style, color)
- [x] `outline-width`
- [x] `outline-style` -- uses same parser as border-style
- [x] `outline-color`
- [x] `outline-offset`

---

## 15. Cursor & Interaction

- [x] `cursor` -- auto, default, pointer, text, move, not-allowed
- [x] `pointer-events` -- auto, none
- [x] `user-select` / `-webkit-user-select` -- auto, none, text, all
- [x] `touch-action` -- auto, none, manipulation, pan-x, pan-y
- [x] `resize` -- none, both, horizontal, vertical
- [x] `caret-color` -- auto or color
- [x] `accent-color` -- auto or color

---

## 16. Scrolling & Snap

### Scroll Behavior

- [x] `scroll-behavior` -- auto, smooth

### Scroll Snap

- [x] `scroll-snap-type` -- stored as string (e.g. "x mandatory")
- [x] `scroll-snap-align` -- stored as string (e.g. "start", "center")
- [x] `scroll-snap-stop` -- normal/always (Cycle 245)

### Scroll Margin

- [x] `scroll-margin` -- 1-4 value shorthand
- [x] `scroll-margin-top` / `scroll-margin-right` / `scroll-margin-bottom` / `scroll-margin-left`
- [x] `scroll-margin-block` / `scroll-margin-inline` -- logical shorthands mapped to physical (Cycle 245)
- [x] `scroll-margin-block-start/end`, `scroll-margin-inline-start/end` -- individual logical longhands (Cycle 245)

### Scroll Padding

- [x] `scroll-padding` -- 1-4 value shorthand
- [x] `scroll-padding-top` / `scroll-padding-right` / `scroll-padding-bottom` / `scroll-padding-left`
- [x] `scroll-padding-inline` -- logical shorthand
- [x] `scroll-padding-block` -- logical shorthand

### Scrollbar

- [x] `scrollbar-color` -- auto or two colors (thumb, track)
- [x] `scrollbar-width` -- auto, thin, none
- [x] `scrollbar-gutter` -- auto, stable, stable both-edges

### Overscroll

- [x] `overscroll-behavior` -- 1-2 value shorthand (auto, contain, none)
- [x] `overscroll-behavior-x`
- [x] `overscroll-behavior-y`

---

## 17. Generated Content

- [x] `content` -- string literals, none, normal, open-quote, close-quote, no-open-quote, no-close-quote, attr(), counter()
- [x] `quotes` -- none, auto, or quoted strings

---

## 18. Image & Object Properties

- [x] `object-fit` -- fill, contain, cover, none, scale-down
- [x] `object-position` -- keywords (left/center/right, top/center/bottom) and percentage
- [x] `image-rendering` -- auto, smooth, high-quality, crisp-edges, pixelated, -webkit-optimize-contrast
- [x] `image-orientation` -- from-image, none, flip

---

## 19. Containment & Visibility

- [x] `contain` -- none, strict, content, size, layout, style, paint
- [x] `contain-intrinsic-size` -- 1-2 value shorthand
- [x] `contain-intrinsic-width` / `contain-intrinsic-height`
- [x] `content-visibility` -- visible, hidden, auto
- [x] `will-change` -- auto or property name(s)

---

## 20. Appearance & Color Scheme

- [x] `appearance` / `-webkit-appearance` -- auto, none, menulist-button, textfield, button
- [x] `color-scheme` -- normal, light, dark, light dark
- [x] `forced-color-adjust` -- auto, none, preserve-parent-color
- [x] `print-color-adjust` / `-webkit-print-color-adjust` -- economy, exact
- [x] `color-interpolation` -- auto, sRGB, linearRGB

---

## 21. Pagination & Breaks

- [x] `break-before` -- auto, avoid, always, page, column, region
- [x] `break-after` -- auto, avoid, always, page, column, region
- [x] `break-inside` -- auto, avoid, avoid-page, avoid-column, avoid-region
- [x] `orphans` -- integer
- [x] `widows` -- integer
- [x] `page-break-before` -- auto, always, avoid, left, right
- [x] `page-break-after` -- auto, always, avoid, left, right
- [x] `page-break-inside` -- auto, avoid
- [x] `page` -- named page string value (Cycle 254)

---

## 22. Box Decoration

- [x] `box-decoration-break` / `-webkit-box-decoration-break` -- slice, clone

---

## 23. Motion & Offsets

- [x] `offset-path` -- none or string (e.g. path())
- [x] `offset-distance` -- length
- [x] `offset-rotate` -- stored as string (e.g. "auto", "auto 45deg")
- [x] `offset` -- shorthand stored as string
- [x] `offset-anchor` -- default "auto", stored as string
- [x] `offset-position` -- default "normal", stored as string

---

## 24. SVG Properties

- [x] `fill` -- none, or color
- [x] `fill-opacity` -- 0.0-1.0
- [x] `stroke` -- none, or color
- [x] `stroke-opacity` -- 0.0-1.0
- [x] `stroke-width`
- [x] `stroke-linecap` -- butt, round, square
- [x] `stroke-linejoin` -- miter, round, bevel
- [x] `stroke-dasharray` -- stored as raw string
- [x] `stroke-dashoffset` -- recognized (parsed at render time)
- [x] `text-anchor` -- start, middle, end
- [x] `paint-order` -- stored as string (e.g. "fill stroke markers")
- [x] `dominant-baseline` -- auto, text-bottom, alphabetic, ideographic, middle, central, mathematical, hanging, text-top
- [x] `fill-rule` -- nonzero (0) / evenodd (1) in ComputedStyle + cascade (Cycle 244)
- [x] `clip-rule` -- nonzero (0) / evenodd (1) in ComputedStyle + cascade (Cycle 244)
- [x] `marker` -- shorthand sets marker-start/mid/end to same value, stored as string
- [x] `marker-start` -- url() reference or "none", stored as string
- [x] `marker-mid` -- url() reference or "none", stored as string
- [x] `marker-end` -- url() reference or "none", stored as string
- [x] `stroke-miterlimit` -- float, default 4.0 in ComputedStyle + cascade (Cycle 244)
- [x] `shape-rendering` -- auto/optimizeSpeed/crispEdges/geometricPrecision (Cycle 244)
- [x] `vector-effect` -- none/non-scaling-stroke (Cycle 244)
- [x] `stop-color` -- full color parsing, ARGB in ComputedStyle (Cycle 244)
- [x] `stop-opacity` -- float clamped 0-1, default 1.0 (Cycle 244)
- [x] `flood-color` -- full color parsing, ARGB in ComputedStyle (default black)
- [x] `flood-opacity` -- float clamped 0-1, default 1.0
- [x] `lighting-color` -- full color parsing, ARGB in ComputedStyle (default white)

---

## 25. Ruby & East Asian Typography

- [x] `ruby-align` -- space-around, start, center, space-between
- [x] `ruby-position` -- over, under, inter-character
- [x] `ruby-overhang` -- auto/none/start/end (Cycle 253)

---

## 26. Math (MathML)

- [x] `math-style` -- normal, compact
- [x] `math-depth` -- auto-add, or integer

---

## 27. WebKit / Vendor Extensions

- [x] `-webkit-text-stroke` -- shorthand (width + color)
- [x] `-webkit-text-stroke-width`
- [x] `-webkit-text-stroke-color`
- [x] `-webkit-text-fill-color`
- [x] `-webkit-box-orient` -- mapped to flex-direction
- [x] `-webkit-line-clamp` / `line-clamp` -- none or integer

---

## 28. Initial Letter (Drop Caps)

- [x] `initial-letter` -- normal, or size + optional sink
- [x] `initial-letter-align` -- auto, border-box, alphabetic

---

## 29. CSS Custom Properties (Variables)

- [x] `--*` (custom properties) -- stored in `custom_properties` map
- [x] `var()` resolution -- with fallback values
- [x] Inheritance of custom properties from parent

---

## 30. Global Keywords

- [x] `inherit` -- supported for many common properties
- [~] `initial` / `unset` / `revert` -- recognized via `all` property but not per-property
- [x] `all` -- stores keyword for global reset (initial, inherit, unset, revert)

---

## 31. CSS Length Units

- [x] `px` -- pixels
- [x] `em` -- relative to element font-size
- [x] `rem` -- relative to root font-size
- [x] `%` -- percentage
- [x] `vw` / `vh` -- viewport width/height
- [x] `vmin` / `vmax` -- viewport min/max
- [x] `ch` -- character width
- [x] `lh` -- line-height
- [x] `calc()` -- full expression tree with +, -, *, /, min(), max(), nested
- [x] `min()` / `max()` / `clamp()` -- via CalcExpr tree
- [x] Advanced calc functions: `mod`, `rem`, `abs`, `sign`, `round`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sqrt`, `pow`, `hypot`, `exp`, `log`
- [x] `dvh` / `dvw` / `svh` / `svw` / `lvh` / `lvw` -- aliased to vh/vw (Cycle 232)
- [x] `cqi` / `cqb` / `cqw` / `cqh` -- mapped to vw/vh as fallback (Cycle 232)

---

## 32. CSS Color Values

- [x] Named colors (140+ CSS named colors via `parse_color`)
- [x] Hex colors -- `#RGB`, `#RRGGBB`, `#RRGGBBAA`
- [x] `rgb()` / `rgba()`
- [x] `hsl()` / `hsla()`
- [x] `transparent`
- [x] `currentColor`
- [x] `color()` -- srgb, srgb-linear, display-p3, a98-rgb color spaces with alpha (verified Cycle 254)
- [x] `lab()` / `lch()` / `oklch()` / `oklab()` -- full CSS Color Level 4 (verified Cycle 232)
- [x] `color-mix()` -- full implementation (verified Cycle 232)
- [x] `hwb()` -- full implementation (verified Cycle 232)
- [x] `light-dark()` -- connected to macOS dark mode (Cycle 224)

---

## 33. At-Rules

- [x] `@import` -- URL extraction and stylesheet fetching
- [x] `@media` -- media queries with rules inside
- [x] `@keyframes` / `@-webkit-keyframes` -- keyframe animation definitions
- [x] `@font-face` -- descriptor parsing (font-family, src, weight, style, display, unicode-range)
- [x] `@supports` -- condition parsed, rules inside
- [x] `@layer` -- named layers parsed, rules inside
- [x] `@container` -- name + condition parsed, rules inside
- [x] `@property` -- custom property registration (inherits, syntax, initial-value)
- [x] `@charset` / `@namespace` / `@page` -- recognized and skipped
- [~] `@font-palette-values` -- parsed and skipped (Cycle 252)
- [~] `@counter-style` -- parsed with name + descriptors map (Cycle 252)
- [x] `@scope` -- parsed with scope_start/scope_end, rules applied (Cycle 252)
- [~] `@starting-style` -- parsed and skipped (Cycle 252)

---

## 34. Container Queries

- [x] `container-type` -- normal, size, inline-size, block-size
- [x] `container-name` -- string
- [x] `container` -- shorthand (name / type)
- [x] `@container` at-rule -- parsed with name and condition

---

## 35. Not Implemented (Notable Omissions)

Remaining gaps (as of Cycle 246):

| Property | Category | Priority |
|----------|----------|----------|
| `margin-trim` | Box Model | Low |
| ~~`clip-path: url()` (SVG ref)~~ | Visual | DONE |
| ~~`mask` shorthand + `mask-origin/position/clip`~~ | Mask | DONE |
| `mask-border` longhands | Mask | Low |
| ~~`shape-outside: polygon()/url()`~~ | Shape | DONE |
| ~~`marker` / `marker-start/mid/end`~~ | SVG | DONE |
| ~~`ruby-overhang`~~ | Typography | DONE |
| ~~`@counter-style`~~ | Counters | DONE |
| ~~`@scope`~~ | Cascade | DONE |
| ~~`@starting-style`~~ | Cascade | DONE |
| ~~`@font-palette-values`~~ | Fonts | DONE |

---

## Summary Statistics

| Category | Implemented | Partial | Not Implemented |
|----------|-------------|---------|-----------------|
| Box Model | 24 | 0 | 9 |
| Border | 36 | 1 | 1 |
| Display & Layout | 18 | 0 | 6 |
| Flexbox | 18 | 0 | 0 |
| Grid | 8 | 0 | 5 |
| Typography & Fonts | 53 | 0 | 3 |
| Backgrounds | 11 | 2 | 1 |
| Visual Effects | 19 | 2 | 7 |
| Transforms | 9 | 0 | 1 |
| Transitions & Animations | 14 | 0 | 6 |
| List & Counters | 5 | 0 | 1 |
| Tables | 5 | 0 | 0 |
| Multi-column | 8 | 0 | 1 |
| Outline | 5 | 0 | 0 |
| Interaction | 7 | 0 | 0 |
| Scrolling | 17 | 0 | 2 |
| Generated Content | 2 | 0 | 0 |
| Image/Object | 4 | 0 | 0 |
| Containment | 5 | 0 | 0 |
| SVG | 11 | 0 | 10 |
| At-Rules | 9 | 0 | 4 |
| **Total** | **~288** | **~5** | **~57** |

**Overall coverage: ~82% of the ~350 most commonly used CSS properties and at-rules.**
