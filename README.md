# from_scratch_browser

`from_scratch_browser` is a C++17 learning project for building a browser engine in small, explicit modules.

Current scope is intentionally narrow:
- Static HTML parsing with document loading from plain local paths, `http://`, `https://`, and `file://`, plus `<base href="...">`-aware relative resource loading.
- CSS from inline/external stylesheets (including `@import`, percent-encoded `data:text/css,...`, and `media="..."` links/styles), with tag/ID/class selectors, quoted/unquoted `[id=...]` / `[class=...]` / `[class~=...]`, descendant/child/adjacent/general-sibling combinators, and pseudo selectors including `:nth-child(...)`, `:empty`, and `:only-child`; color parsing includes hex, `rgb(%)`, `rgba(...)`, `hsl(...)`, `hsla(...)`, named colors, `transparent`, and `currentColor`.
- JavaScript subset support for `document.getElementById(...)`, `document.querySelector("#...")`, and `document.body.style...` with `innerText`, `textContent`, `style.{color,background,backgroundColor,border,borderColor,borderWidth,borderStyle,fontSize}`, and `style = "..."`.
- Layout and paint flow for a limited, non-web-platform-complete subset.
- `examples/sample.html` includes fixtures for local `@import`, data-URL styles, media-qualified `<link>` and `<style>`, class/attribute selectors (including `:not(...)`, `:first-of-type`, `:last-of-type`, `:nth-of-type(...)`, `:nth-last-child(...)`, `[attr]`, and `[attr^=]` / `[attr$=]` / `[attr*=]` with fallback classes, plus entity-decoded ID values including named `&apos;`, `&copy;`, `&reg;`, `&trade;`, `&cent;`, `&pound;`, `&euro;`, `&yen;`, `&sect;`, `&deg;`, `&ndash;`, `&mdash;`, and numeric `&#...;`/`&#x...;` forms), `currentColor`/transparent/`rgb(%)`/`hsla(...)`, `:nth-child(...)` / `:empty` / `:only-child`, case-insensitive/spaced `:NTH-LAST-CHILD( 2 )` normalization coverage with fallback, runtime `style = "..."`, runtime `.setAttribute("style", "...")`, runtime non-style `.setAttribute("class", "...")`, runtime `.setAttribute("id", "...")`, runtime `.className = "..."`, runtime `.id = "..."`, runtime `document.body.className/id` mutation selectors, runtime `document.body.setAttribute/removeAttribute` transitions, and runtime `.removeAttribute("style"|"class")` fallback transitions with visible selector-driven styling impact.
- `scripts/run_sample.sh` and `scripts/smoke_test.sh` exercise sample rendering over HTTP and direct local-path input (without `file://`).

## Build

### Requirements
- CMake 3.16 or newer
- A C++17 compiler (Clang, GCC, or MSVC)
- OpenSSL is optional (linked automatically when found)

### Configure and compile

```bash
cmake -S . -B build
cmake --build build
```

If `src/main.cpp` is not present yet, the build still succeeds using a generated bootstrap `main` file so the toolchain can be validated early.

## Run

```bash
./build/from_scratch_browser https://example.com output.ppm 1280 720
```

On multi-config generators (for example Visual Studio), run the binary from the selected config directory such as `build/Debug/`.

Current run flow is:
`fetch -> parse -> CSS -> layout -> render -> PPM`

For an end-to-end example run, use the sample run script in `scripts/` (when present in your checkout).

## Project layout

Headers are under `include/`, and implementation modules are grouped under `src/`:

- `src/browser`: application orchestration pipeline
- `src/core`: shared configuration and core runtime pieces
- `src/html`: HTML tokenizer/parser for the supported static subset
- `src/css`: CSS tokenizer/parser and style resolution
- `src/js`: starter JavaScript subset parsing/runtime support
- `src/layout`: box tree construction and layout calculations
- `src/render`: painting and output surface integration
- `src/net`: optional network/TLS fetch helpers and local file loading
- `src/utils`: utility helpers used across modules

The executable target is named `from_scratch_browser`.
