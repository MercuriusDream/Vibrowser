# HTML5 Elements Standards Checklist

> Auto-generated checklist of all standard HTML5 elements vs. Clever browser engine implementation.
> Source of truth: [MDN HTML Element Reference](https://developer.mozilla.org/en-US/docs/Web/HTML/Element)
>
> Implementation checked against:
> - `src/paint/render_pipeline.cpp` (layout tree building, element-specific rendering)
> - `src/css/style/computed_style.cpp` (UA default styles per tag)
> - `src/css/style/style_resolver.cpp` (CSS cascade with tag defaults)
>
> Legend:
> - [x] = Implemented (has explicit handling or UA default styles)
> - [~] = Partial (placeholder, basic structure, or incomplete semantics)
> - [ ] = Not implemented (no specific handling; falls through to generic inline/block)

---

## Main Root

- [x] `<html>` -- block display; dark mode color-scheme support; background/text color defaults

## Document Metadata

- [x] `<base>` -- parsed for base URL resolution (`find_all_elements("base")`)
- [x] `<head>` -- skipped from rendering (display:none)
- [x] `<link>` -- parsed for external stylesheets and favicon; skipped from rendering
- [x] `<meta>` -- parsed for charset, viewport, theme-color, description; skipped from rendering
- [x] `<style>` -- parsed for embedded CSS stylesheets; skipped from rendering
- [x] `<title>` -- parsed for window title; skipped from rendering

## Sectioning Root

- [x] `<body>` -- block display; legacy bgcolor/text attributes; dark mode color-scheme support

## Content Sectioning

- [x] `<address>` -- block display; italic font style
- [x] `<article>` -- block display (UA default); block tag whitespace suppression
- [x] `<aside>` -- block display (UA default); block tag whitespace suppression
- [x] `<footer>` -- block display (UA default); block tag whitespace suppression
- [x] `<header>` -- block display (UA default); block tag whitespace suppression
- [x] `<h1>` -- block display; bold 32px; margin defaults
- [x] `<h2>` -- block display; bold 24px; margin defaults
- [x] `<h3>` -- block display; bold 18.72px; margin defaults
- [x] `<h4>` -- block display; bold 16px; margin defaults
- [x] `<h5>` -- block display; bold 13.28px; margin defaults
- [x] `<h6>` -- block display; bold 10.72px; margin defaults
- [x] `<hgroup>` -- block display; heading group container
- [x] `<main>` -- block display (UA default); block tag whitespace suppression
- [x] `<nav>` -- block display (UA default); block tag whitespace suppression
- [x] `<section>` -- block display (UA default); block tag whitespace suppression
- [x] `<search>` -- block display in UA stylesheet (Cycle 242)

## Text Content

- [x] `<blockquote>` -- block display; 40px left margin; 3px left border; 12px left padding
- [x] `<dd>` -- block display; 40px left margin default
- [x] `<div>` -- block display (UA default); used as generic block container
- [x] `<dl>` -- block display; definition list container
- [x] `<dt>` -- block display; bold font-weight default
- [x] `<figcaption>` -- block display; caption for figure elements
- [x] `<figure>` -- block display; 16px top/bottom margin; 40px left/right margin
- [x] `<hr>` -- block display; inset border (1px #CCC/#EEE); legacy color/size/width/noshade attributes
- [x] `<li>` -- list-item display; bullet/number markers; supports ordered list numbering (decimal, lower-alpha, upper-alpha, lower-roman, upper-roman, disc, circle, square, none); start/value attributes
- [x] `<menu>` -- block display with ul-style defaults (margin, padding, list-style-type: disc) (Cycle 242)
- [x] `<ol>` -- block display; decimal list-style-type default; padding-left for indent
- [x] `<p>` -- block display (UA default); block tag whitespace suppression
- [x] `<pre>` -- block display; monospace font; white-space:pre; preserves whitespace
- [x] `<ul>` -- block display; disc list-style-type default; padding-left for indent

## Inline Text Semantics

- [x] `<a>` -- inline display; blue color (#0000EE); underline text-decoration; href resolution; target attribute
- [x] `<abbr>` -- inline display; dotted underline; title attribute stored for tooltip
- [x] `<b>` -- inline display; bold font-weight (700) via UA defaults
- [x] `<bdi>` -- inline display; bidirectional isolation marker
- [x] `<bdo>` -- inline display; bidirectional override; dir attribute (rtl/ltr)
- [x] `<br>` -- inline line break; newline text node; legacy clear attribute
- [x] `<cite>` -- inline display; italic font style
- [x] `<code>` -- inline display; monospace font; 0.9x font size; light gray background (#F5F5F5)
- [x] `<data>` -- inline display; value attribute stored
- [x] `<dfn>` -- inline display; italic font style
- [x] `<em>` -- inline display; italic font-style via UA defaults
- [x] `<i>` -- inline display; italic font-style via UA defaults
- [x] `<kbd>` -- inline display; monospace font; 0.9x size; 1px border; 3px border-radius; gray background
- [x] `<mark>` -- inline display; yellow background (#FFFF00); black text
- [x] `<q>` -- inline display; inline quotation marker
- [x] `<rp>` -- display:none (hidden when ruby is supported)
- [x] `<rt>` -- ruby text; 50% parent font size
- [x] `<ruby>` -- ruby annotation container for East Asian typography
- [x] `<s>` -- inline display; line-through text-decoration
- [x] `<samp>` -- inline display; monospace font; 0.9x size; light gray background
- [x] `<small>` -- inline display; 0.83x font size
- [x] `<span>` -- inline display (UA default); generic inline container
- [x] `<strong>` -- inline display; bold font-weight (700) via UA defaults
- [x] `<sub>` -- inline display; 0.83x font size; vertical offset down
- [x] `<sup>` -- inline display; 0.83x font size; vertical offset up
- [x] `<time>` -- inline display; datetime attribute stored
- [x] `<u>` -- inline display; underline text-decoration via UA defaults
- [x] `<var>` -- inline display; italic font style
- [x] `<wbr>` -- inline display; zero-width space break opportunity marker

## Image and Multimedia

- [x] `<area>` -- display:none; stores shape/coords/href for image map regions
- [x] `<audio>` -- inline-block; media placeholder with play icon and time display; controls attribute; 300x32 default; hidden without controls per spec
- [x] `<img>` -- inline-block (replaced); fetches and decodes real images (stb_image); width/height attributes; alt text fallback; broken image placeholder; object-fit/object-position support; aspect ratio preservation
- [x] `<map>` -- display:none; named image map container; processes child area elements
- [~] `<track>` -- PARTIAL (no explicit handling; falls through as generic element; used implicitly inside video/audio)
- [x] `<video>` -- inline-block; media placeholder with play button triangle; 300x150 default; black background; src attribute

## Embedded Content

- [x] `<embed>` -- inline-block; placeholder (plugin content not supported); 300x150 default; gray background with border
- [ ] `<fencedframe>` -- NOT IMPLEMENTED (experimental; no handling)
- [x] `<iframe>` -- inline-block; placeholder with src attribute; 300x150 default; gray background with border; nested browsing not yet supported
- [x] `<object>` -- inline-block; placeholder with fallback children rendered; 300x150 default
- [x] `<picture>` -- inline-block; selects best source from child `<source>` elements; falls back to child `<img>` src; srcset parsing (first candidate); full image decode and display
- [x] `<source>` -- skipped from rendering outside `<picture>`; srcset attribute parsed within picture context

## SVG and MathML

- [x] `<svg>` -- inline-block; full SVG rendering with viewBox, width/height; child elements: rect, circle, ellipse, line, path, text, tspan, polygon, polyline, g, defs, use, linearGradient, radialGradient, clipPath, stop
- [~] `<math>` -- renders as inline-block container, no MathML layout (Cycle 253)

## Scripting

- [x] `<canvas>` -- inline-block; 300x150 default per spec; JS canvas buffer support via data-canvas-buffer-ptr; white background
- [x] `<noscript>` -- skipped from rendering (JS engine is enabled)
- [x] `<script>` -- parsed and executed by JS engine (QuickJS); skipped from rendering

## Demarcating Edits

- [x] `<del>` -- inline display; line-through text-decoration
- [x] `<ins>` -- inline display; underline text-decoration

## Table Content

- [x] `<caption>` -- block display; centered text; bold; padding
- [x] `<col>` -- display:none; stores span/width/bgcolor metadata for column styling
- [x] `<colgroup>` -- display:none; column group container
- [x] `<table>` -- table display mode; border; legacy attributes (bgcolor, width, align, cellpadding, cellspacing, frame, rules); border-collapse/border-spacing support
- [x] `<tbody>` -- block display; pass-through container
- [x] `<td>` -- block display (flex item); border; padding; colspan/rowspan; legacy bgcolor/align/valign/width attributes
- [x] `<tfoot>` -- block display; pass-through container
- [x] `<th>` -- block display (flex item); bold font-weight; border; padding; colspan/rowspan; legacy attributes
- [x] `<thead>` -- block display; pass-through container
- [x] `<tr>` -- flex display (row direction); legacy bgcolor/align attributes

## Forms

- [x] `<button>` -- inline-block; styled with border, padding, background; dark mode support; renders child content
- [x] `<datalist>` -- display:none; collects option values; stored for input[list] association
- [x] `<fieldset>` -- block display; 2px groove border; padding (0.35em/0.75em/0.625em); 2px horizontal margin
- [x] `<form>` -- block display; form context tracking; collects fields (input/textarea/select/optgroup/option); GET query string building; POST form submission support
- [x] `<input>` -- inline-block (replaced); supports types: text, password, email, search, url, number, tel (text inputs with placeholder); submit, button, reset (button styling with form submission); checkbox, radio (custom painted with accent-color); range (slider with min/max/value); color (color swatch picker); file (Choose File button); date, time, datetime-local, week, month (date/time with placeholder); dark mode support
- [x] `<label>` -- inline display; for attribute stored for association
- [x] `<legend>` -- block display; caption for fieldset; padding
- [x] `<meter>` -- inline-block; colored bar (green/yellow/red based on low/high/optimum algorithm per HTML spec); 200x16 default; dark mode support
- [x] `<optgroup>` -- rendered within select; label displayed as bold header; disabled state; indented child options
- [x] `<option>` -- rendered within select; selected state highlighting; disabled state (gray text); text extraction
- [x] `<output>` -- inline display; for attribute stored
- [x] `<progress>` -- inline-block; determinate (fill bar with ratio) and indeterminate (striped) states; accent-color support; 200x16 default; dark mode
- [x] `<select>` -- inline-block; dropdown mode (single) and listbox mode (multiple/size); option collection; optgroup support; selected/disabled states; dropdown arrow; dark mode; visible_rows; form field tracking
- [ ] `<selectedcontent>` -- NOT IMPLEMENTED (experimental; no handling)
- [x] `<textarea>` -- inline-block; 300x80 default; border; padding; placeholder support; text content display; dark mode

## Interactive Elements

- [x] `<details>` -- block display; toggle support (open attribute + interactive toggle state); disclosure triangle (arrow); renders summary when closed, all children when open; border and padding
- [x] `<dialog>` -- block display; open/closed state; centered overlay (position:absolute); modal support (data-modal); default border, padding, white background; 600px max-width
- [ ] `<geolocation>` -- NOT IMPLEMENTED (experimental; no handling)
- [x] `<summary>` -- block display; rendered as first child of details; disclosure triangle prepended; click toggles parent details

## Web Components

- [x] `<slot>` -- inline display; Web Components slot placeholder; name attribute; renders fallback children
- [x] `<template>` -- skipped from rendering (display:none); used as inert template container

## Deprecated / Obsolete Elements

- [x] `<acronym>` -- DEPRECATED; inline display; dotted underline; title tooltip (same as abbr)
- [x] `<big>` -- DEPRECATED; inline display; 1.17x font size
- [x] `<center>` -- DEPRECATED; block display; text-align:center
- [ ] `<content>` -- OBSOLETE; NOT IMPLEMENTED
- [ ] `<dir>` -- DEPRECATED; NOT IMPLEMENTED (no tag-specific handling)
- [x] `<font>` -- DEPRECATED; inline display; legacy color/size/face attributes
- [ ] `<frame>` -- DEPRECATED; NOT IMPLEMENTED
- [ ] `<frameset>` -- DEPRECATED; NOT IMPLEMENTED
- [ ] `<image>` -- OBSOLETE; NOT IMPLEMENTED (non-standard synonym for img)
- [x] `<marquee>` -- DEPRECATED; block display; direction attribute; bgcolor; static representation (no animation)
- [ ] `<menuitem>` -- DEPRECATED; NOT IMPLEMENTED
- [ ] `<nobr>` -- DEPRECATED; NOT IMPLEMENTED
- [ ] `<noembed>` -- DEPRECATED; NOT IMPLEMENTED
- [ ] `<noframes>` -- DEPRECATED; NOT IMPLEMENTED
- [ ] `<param>` -- DEPRECATED; NOT IMPLEMENTED
- [ ] `<plaintext>` -- DEPRECATED; NOT IMPLEMENTED
- [ ] `<rb>` -- DEPRECATED; NOT IMPLEMENTED
- [ ] `<rtc>` -- DEPRECATED; NOT IMPLEMENTED
- [ ] `<shadow>` -- OBSOLETE; NOT IMPLEMENTED
- [x] `<strike>` -- DEPRECATED; inline display; line-through text-decoration (same as del/s)
- [x] `<tt>` -- DEPRECATED; inline display; monospace font; 0.9x size; gray background (same as code/samp)
- [ ] `<xmp>` -- DEPRECATED; NOT IMPLEMENTED

---

## Summary Statistics

### Current Standard Elements (117 non-experimental + 3 experimental = 120 per MDN)

| Status | Count | Percentage (of 117) |
|--------|-------|---------------------|
| Implemented [x] | 110 | 94.0% |
| Partial [~] | 1 | 0.9% |
| Not Implemented [ ] | 3 | 2.6% |
| Experimental (not targeted) | 3 | 2.6% |

**Missing non-experimental standard elements:**
1. `<search>` -- Content sectioning (new semantic element, easy to add as block)
2. `<menu>` -- Text content (context menu / toolbar list, easy to add as ul-like block)
3. `<math>` -- MathML (mathematical notation, significant effort)

**Partial implementation:**
1. `<track>` -- Multimedia (subtitle/caption tracks; no explicit handling but implicitly part of video/audio)

**Experimental elements (not targeted):**
1. `<fencedframe>` -- Embedded content (Privacy Sandbox API)
2. `<selectedcontent>` -- Forms (custom select rendering)
3. `<geolocation>` -- Interactive (Geolocation API element)

### Deprecated/Obsolete Elements (22 total)

| Status | Count |
|--------|-------|
| Implemented (compat) | 8 |
| Not Implemented | 14 |

### Notable Implementation Highlights

- **Full image pipeline**: `<img>`, `<picture>`, `<source>` with real image fetching/decoding via stb_image
- **Complete form support**: All major input types (text, password, email, checkbox, radio, range, color, date/time, file, submit/button/reset), select (dropdown + listbox), textarea, datalist, fieldset/legend, form submission (GET + POST)
- **SVG rendering**: rect, circle, ellipse, line, path, text, tspan, polygon, polyline, g, defs, use, gradients, clipPath
- **Interactive elements**: details/summary toggle, dialog (modal + non-modal)
- **Full table model**: table, thead, tbody, tfoot, tr, td, th, caption, col, colgroup with colspan/rowspan, legacy attributes
- **Media placeholders**: video (play button), audio (controls bar), canvas (JS buffer support)
- **Complete inline text semantics**: All 30 elements handled via UA defaults or explicit render_pipeline code
- **CSS cascade integration**: UA default styles in computed_style.cpp, with full CSS cascade override support
- **Dark mode**: color-scheme support on html, body, input, textarea, select, button, progress, meter
