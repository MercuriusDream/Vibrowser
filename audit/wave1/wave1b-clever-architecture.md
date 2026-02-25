# Wave1-B: Clever Architecture Reconstruction (`clever/src/{url,ipc,platform,net,dom,html}`)

## Scope
- Primary modules: `clever/src/url`, `clever/src/ipc`, `clever/src/platform`, `clever/src/net`, `clever/src/dom`, `clever/src/html`
- Data-flow reconstruction target: `URL -> net -> dom -> style -> layout -> paint`
- Supporting runtime integration reviewed: `clever/src/shell/browser_window.mm`, `clever/src/css/style/style_resolver.cpp`, `clever/src/paint/render_pipeline.cpp`, relevant CMake targets under `clever/src/*/CMakeLists.txt`
- Finding row schema used here matches the existing Wave1 matrix fields (`id`, `path:line`, `status`, `spec_citation`, `certainty`, `impact`, `scope_overlap`).

## Module Boundary Reconstruction

### Build-time boundaries (CMake)
| Module | Target | Declared deps | Evidence |
|---|---|---|---|
| URL | `clever_url` | ICU/PkgConfig ICU | `clever/src/url/CMakeLists.txt:1-16` |
| Platform | `clever_platform` | `Threads::Threads` | `clever/src/platform/CMakeLists.txt:1-12` |
| IPC | `clever_ipc` | `clever_platform` | `clever/src/ipc/CMakeLists.txt:1-11` |
| Net | `clever_net` | `clever_url`, `clever_platform`, `ZLIB::ZLIB` | `clever/src/net/CMakeLists.txt:1-20` |
| DOM | `clever_dom` | none | `clever/src/dom/CMakeLists.txt:1-9` |
| HTML | `clever_html` | `clever_dom` | `clever/src/html/CMakeLists.txt:1-7` |
| Style | `clever_css_style` | `clever_css_parser`, `clever_dom` | `clever/src/css/style/CMakeLists.txt:1-8` |
| Layout | `clever_layout` | `clever_css_style` | `clever/src/layout/CMakeLists.txt:1-6` |
| Paint | `clever_paint` | `clever_layout`, `clever_html`, `clever_css_style`, `clever_net`, `clever_js` | `clever/src/paint/CMakeLists.txt:1-21` |

### Runtime boundaries (observed)
1. Shell navigation path starts in `BrowserWindow::navigateToURL`, normalizes user input, and dispatches background fetch (`clever/src/shell/browser_window.mm:688-739`).
2. Network fetch path uses `net::HttpClient` + `net::Request::parse_url` + shared `CookieJar` (`clever/src/shell/browser_window.mm:742-808`, `clever/src/net/request.cpp:37-94`, `clever/src/net/http_client.cpp:531-681`, `clever/src/net/cookie_jar.cpp:26-139`).
3. Render entrypoint calls `paint::render_html(html, base_url, width, height)` (`clever/src/shell/browser_window.mm:869-883`).
4. `render_html` pipeline stages are explicit:
   - Parse HTML to `html::SimpleNode` (`clever/src/paint/render_pipeline.cpp:10501-10503`)
   - Build style resolver + fetch linked/inline CSS (`clever/src/paint/render_pipeline.cpp:10630-10707`)
   - Build styled layout tree (`clever/src/paint/render_pipeline.cpp:11042-11044`, `5180-5664`, `5833-5843`)
   - Layout compute (`clever/src/paint/render_pipeline.cpp:11051-11064`)
   - Paint display list (`clever/src/paint/render_pipeline.cpp:11125-11127`)
   - Raster to software buffer (`clever/src/paint/render_pipeline.cpp:11144-11147`)

## Data Flow Reconstruction (`URL -> net -> dom -> style -> layout -> paint`)
1. **URL**
   - User input is normalized in shell (`http://` auto-prefix, inline HTML bypass) (`clever/src/shell/browser_window.mm:703-715`).
   - URL module provides a WHATWG-style parser API (`clever::url::parse`) (`clever/src/url/url_parser.cpp:321-589`, `clever/include/clever/url/url.h:24-25`).
   - Active runtime fetch path does not call `clever::url::parse`; it calls `Request::parse_url` string parsing (`clever/src/net/request.cpp:37-94`).

2. **net**
   - `HttpClient::fetch` handles URL parsing fallback, cache lookup/revalidation, redirects, and cache store (`clever/src/net/http_client.cpp:531-681`).
   - Response parsing handles headers, chunked/content-length framing, and gzip/deflate decompression (`clever/src/net/response.cpp:148-260`).
   - Cookie attachment and persistence are handled through shared `CookieJar` (`clever/src/net/cookie_jar.cpp:31-139`).

3. **dom/html parse layer**
   - HTML parser returns a `SimpleNode` tree (`clever/src/html/tree_builder.cpp:854-890`, `clever/include/clever/html/tree_builder.h:12-33,103`).
   - Conversion hooks to full `dom::Document` exist (`to_dom_document`, `to_simple_node`) (`clever/src/html/tree_builder.cpp:892-905`).
   - Runtime render path uses `SimpleNode` directly (`clever/src/paint/render_pipeline.cpp:10502`, `5458`, `11223`).

4. **style**
   - `StyleResolver` collects matching rules, computes specificity/source-order precedence, and resolves computed style (`clever/src/css/style/style_resolver.cpp:5773-5935`, `5937-5957`).
   - Render pipeline fetches external stylesheets, processes `@import`, and adds UA + page stylesheets before layout-tree construction (`clever/src/paint/render_pipeline.cpp:10668-10707`, `10407-10448`).

5. **layout**
   - `build_layout_tree_styled` resolves style per element and maps `ComputedStyle` into `LayoutNode` fields (`clever/src/paint/render_pipeline.cpp:5660-5664`, `5833-6095`).
   - Layout compute runs via `layout::LayoutEngine::compute` (`clever/src/paint/render_pipeline.cpp:11051-11064`).

6. **paint**
   - `Painter::paint` emits display list from computed layout tree (`clever/src/paint/render_pipeline.cpp:11125-11127`).
   - `SoftwareRenderer` rasterizes the display list into pixel buffer (`clever/src/paint/render_pipeline.cpp:11144-11147`).

## Findings Matrix
| id | path:line | status | spec_citation | certainty | impact | scope_overlap |
|---|---|---|---|---|---|---|
| W1B-MB-01 | clever/CMakeLists.txt:73 | Implemented | Module map and phased boundaries (`CLEVER_ENGINE_BLUEPRINT.md:75-90`) | High | Medium | module_boundaries |
| W1B-MB-02 | clever/src/net/CMakeLists.txt:16 | Partial | Platform+Net separation intent (`CLEVER_ENGINE_BLUEPRINT.md:79,82,111-115,137-143`) | High | Medium | module_boundaries,platform,net |
| W1B-FLOW-01 | clever/src/shell/browser_window.mm:688 | Implemented | Data-flow entry starts with URL input (`CLEVER_ENGINE_BLUEPRINT.md:1425`) | High | High | url,net,pipeline |
| W1B-URL-01 | clever/src/url/url_parser.cpp:321 | Implemented | URL parser module scope (`CLEVER_ENGINE_BLUEPRINT.md:203-216`) | High | Medium | url |
| W1B-URL-02 | clever/src/net/request.cpp:37 | Missing | "Every URL in the engine flows through" URL parser + `Request` uses `url::URL` (`CLEVER_ENGINE_BLUEPRINT.md:207,269-277`) | High | High | url,net |
| W1B-URL-03 | clever/src/paint/render_pipeline.cpp:4798 | Missing | Central relative URL resolution expected in URL module (`CLEVER_ENGINE_BLUEPRINT.md:207,215`) | High | Medium | url,paint,net |
| W1B-NET-01 | clever/src/net/http_client.cpp:531 | Implemented | Networking lifecycle scope (fetch, redirect, cookies) (`CLEVER_ENGINE_BLUEPRINT.md:261-266`) | High | High | net |
| W1B-NET-02 | clever/src/paint/render_pipeline.cpp:4837 | Missing | Renderer should not directly touch network; IPC-mediated resource flow (`CLEVER_ENGINE_BLUEPRINT.md:65,1429-1436`) | High | High | net,ipc,paint |
| W1B-HTML-01 | clever/src/html/tree_builder.cpp:854 | Implemented | HTML tokenizer/tree-builder pipeline (`CLEVER_ENGINE_BLUEPRINT.md:340-345,386-388`) | High | Medium | html |
| W1B-DOM-01 | clever/src/paint/render_pipeline.cpp:10502 | Partial | HTML parse target is DOM tree (`CLEVER_ENGINE_BLUEPRINT.md:340-341,379`) vs runtime `SimpleNode` use | High | High | html,dom,pipeline |
| W1B-DOM-02 | clever/include/clever/dom/node.h:15 | Missing | Dirty-flag incremental recompute protocol (`CLEVER_ENGINE_BLUEPRINT.md:491-501,1443-1444`) | Medium | Medium | dom,style,layout,paint |
| W1B-STYLE-01 | clever/src/css/style/style_resolver.cpp:5773 | Implemented | Style resolution architecture (cascade/specificity/inheritance) (`CLEVER_ENGINE_BLUEPRINT.md:704-711`) | High | High | style |
| W1B-STYLE-02 | clever/src/paint/render_pipeline.cpp:5663 | Implemented | Layout tree built from computed styles (`CLEVER_ENGINE_BLUEPRINT.md:782-783,807-809`) | High | High | style,layout |
| W1B-LAYOUT-01 | clever/src/paint/render_pipeline.cpp:11051 | Implemented | Stage ordering style->layout->paint (`CLEVER_ENGINE_BLUEPRINT.md:69,1438-1440`) | High | High | layout,paint |
| W1B-PAINT-01 | clever/src/paint/render_pipeline.cpp:11126 | Implemented | Paint/display-list and raster stages (`CLEVER_ENGINE_BLUEPRINT.md:960,983-987,1439-1441`) | High | High | paint |
| W1B-IPC-01 | clever/src/shell/browser_window.mm:742 | Missing | Navigation/resource exchange should cross IPC boundary (`CLEVER_ENGINE_BLUEPRINT.md:161-162,186-189,1426,1429`) | High | High | ipc,net,pipeline |
| W1B-PLATFORM-01 | clever/src/shell/browser_window.mm:737 | Partial | Platform event-loop/thread role in pipeline orchestration (`CLEVER_ENGINE_BLUEPRINT.md:124,138-143`) | Medium | Medium | platform,pipeline |

## Notes
- `clever::url::parse` and `url::URL` are implemented in `src/url`, but active runtime fetch/render paths are driven by `net::Request::parse_url` and local `resolve_url` helpers.
- IPC and platform modules are present and tested as standalone components, but the observed runtime page-load/render chain is single-process and directly network-coupled.
