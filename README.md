<p align="center">
  <a href="https://mercuriusdream.com/Vibrowser/" target="_blank" rel="noreferrer">
    <img src="https://mercuriusdream.com/Vibrowser/favicon.ico" alt="Vibrowser website logo" width="72" height="72" />
  </a>
  <br />
  <a href="https://mercuriusdream.com/Vibrowser/" target="_blank" rel="noreferrer">
    <img src="https://img.shields.io/badge/Open%20Vibrowser%20Website-ff4f8b?style=for-the-badge&logo=googlechrome&logoColor=white&labelColor=0a0a0a" alt="Open the Vibrowser website" />
  </a>
</p>

![From Scratch](https://img.shields.io/badge/Mode-From--Scratch%20Browser-ff4f8b?style=flat-square)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/Build-CMake-064F8C?style=flat-square&logo=cmake&logoColor=white)
![Website](https://img.shields.io/website?url=https%3A%2F%2Fmercuriusdream.com%2FVibrowser%2F&style=flat-square&up_message=online&down_message=offline&logo=googlechrome&label=Live%20Site&color=06b6d4)

<h1 align="center">Vibrowser</h1>
<p align="center"><strong>Open-source, from-scratch browser engineering in public.</strong></p>

<p align="center">
  <a href="https://mercuriusdream.com/Vibrowser/">
    <img src="https://img.shields.io/badge/Visit%20Website-ff4f8b?style=for-the-badge&logo=googlechrome&logoColor=white&labelColor=0a0a0a" alt="Visit website">
  </a>
  <a href="#project-snapshot">
    <img src="https://img.shields.io/badge/Project%20Snapshot-1f2937?style=for-the-badge&logo=markdown&logoColor=white" alt="Project snapshot">
  </a>
  <a href="#landing-page-vite--react">
    <img src="https://img.shields.io/badge/Landing%20Demo-06b6d4?style=for-the-badge&logo=vite&logoColor=white&labelColor=0f172a" alt="Landing page">
  </a>
</p>

<img width="1023" height="802" alt="image" src="https://github.com/user-attachments/assets/d0b873b2-3c8f-4195-8890-be1014ebe8b9" />

## 🎯 Project Snapshot

| What | Details |
| --- | --- |
| Project | From-scratch C++17 browser engine |
| Domain | Browser rendering + parsing + JS runtime internals |
| Live Demo | https://mercuriusdream.com/Vibrowser/ |
| Primary Goal | Learnable, inspectable, reproducible browser engineering |

It is a small, opinionated, VIBECODED, from-scratch C++17 engine that exists to prove each browser subsystem can be built, understood, and tuned by hand, one layer at a time.  
The project is intentionally pragmatic: no magic black boxes, no hand-wavey abstractions, and no pretending it is production-ready before it is.

The executable is still `from_scratch_browser`, but the spirit is vibey, curious, and unapologetically experimental.

---

## Open-Source Mission

Vibrowser is an OSS project for people who want to understand browser internals by building them directly:

- network/input ingestion
- HTML parsing
- CSS parsing + selector matching
- style resolution
- layout
- paint output

The goal is not to hide complexity. The goal is to make complexity inspectable.

## Development Machine Specs (neofetch)

Current local build machine used for active development:

- OS: `macOS 26.3 25D125 arm64`
- Host: `MacBookPro18,3`
- Kernel: `25.3.0`
- Shell: `zsh 5.9`
- Terminal: `Codex`
- CPU: `Apple M1 Pro`
- GPU: `Apple M1 Pro`
- Memory: `16GB` (`16384MiB`)

To print your current machine profile:

```bash
neofetch
```

## Landing Page (Vite + React)

A dedicated landing page app exists in `landing/` and is ready for GitHub Pages deployment.

```bash
cd landing
bun install
bun run dev
```

Deployment options:

- Manual deploy to `gh-pages` branch:
  - `bun run deploy`
- Automatic deploy via GitHub Actions:
  - `.github/workflows/landing-pages.yml`

---

## This Is Not (Just) a Toy

Most browser projects start with a rendering toy and drift into mystery.  
Vibrowser does the opposite.

Every stage is explicit and testable:

1. Input acquisition
2. Document parsing
3. CSS processing
4. Style resolution
5. Layout tree construction
6. Paint and output generation

If a stage fails, you can isolate it in code, logs, and generated fixture output.  
That is the base contract of this repo.

Current design intention:

- Build confidence through deterministic artifacts (`.ppm` images, script-driven smoke checks).
- Keep implementation boundaries narrow.
- Expand coverage through concrete fixtures in `examples/` and `tests/`.
- Keep the API surface honest, small, and easy to reason about.

---

## What Vibrowser Can Already Do

This section is not a marketing list.
It is the practical boundary of current behavior and what your current workspace expects.

### Document Fetch and Inputs

- Load documents from:
  - plain local paths like `examples/sample.html`
  - `http://` and `https://`
  - `file://` URLs
- Resolve relative resources using `<base href="...">`.
- Handle direct HTTP rendering through local and networked fetch helpers.

### HTML Parsing

- Parse static HTML into a document structure suitable for downstream layout.
- Support for a learning-oriented subset of markup sufficient for style and script exercise.
- Keep parsing deterministic for fixture-driven regressions.

### CSS Parsing and Styling

- Parse inline and external stylesheets.
- Resolve `@import`, with `media` support and data URLs (`data:text/css,...`) for experiments.
- Handle:
  - IDs, classes, and tag selectors
  - `[id=...]`, `[class=...]`, `[class~=...]`
  - `[attr]`, `[attr^=...]`, `[attr$=...]`, `[attr*=...]`
  - Attribute value entities and numeric/named conversions used in fixtures
  - Combinators:
    - descendant
    - child
    - adjacent sibling
    - general sibling
  - pseudo selectors:
    - `:nth-child(...)`
    - `:nth-last-child(...)`
    - `:empty`
    - `:only-child`
    - `:first-of-type`
    - `:last-of-type`
    - `:not(...)`
- Color parsing coverage for:
  - hex
  - rgb / rgba
  - hsl / hsla
  - named colors
  - `transparent`
  - `currentColor`
  - short-form hex alpha usage

### JS Subset Runtime

- `document.getElementById(...)`
- `document.querySelector("#...")`
- `document.body.style` style mutation
- direct style assignment and fallback style mutations
- `setAttribute` / `removeAttribute` transitions for class, id, and style
- selected runtime style properties:
  - `color`
  - `background`
  - `backgroundColor`
  - `border`
  - `borderColor`
  - `borderWidth`
  - `borderStyle`
  - `fontSize`

### Layout, Render, and Output

- Box tree generation from parsed DOM + computed style context
- Layout pass that feeds renderer pipeline
- Rasterized frame output to `.ppm` for deterministic visual checks
- Minimal but explicit paint model for current feature set

### Feature Validation Assets

- `examples/sample.html` is the primary fixture file for wide selector and runtime behavior checks.
- `scripts/run_sample.sh` exercises render flow for:
  - HTTP input
  - local path input
  - file URL input
- `scripts/smoke_test.sh` validates core pipeline stability and fixture color assertions.

---

## Pipeline Snapshot

The mental model for every render run:

`fetch` → `parse` → `style` → `layout` → `paint` → `ppm output`

That is the entire product.

If an artifact fails:

1. isolate the phase
2. run the smallest fixture that reaches it
3. compare generated output and logs
4. fix only the violating stage

---

## Project Layout

`include/` and `src/` are the main code map.

- `src/browser`  
  Browser orchestration and top-level flow

- `src/core`  
  Shared runtime state and helpers

- `src/html`  
  HTML tokenizer/parser and document handling

- `src/css`  
  CSS tokenizer/parser + selector and declaration processing

- `src/js`  
  JavaScript subset parsing and DOM-facing runtime helpers

- `src/layout`  
  Layout tree construction and measurement model

- `src/render`  
  Painting and image output conversion

- `src/net`  
  Networking helpers (including HTTP/TLS integrations)

- `src/utils`  
  Cross-module utility functions

- `src/engine`, `src/app`  
  Application and engine-level glue points

`tests/` contains `test_*.cpp` executables configured via CMake.

`examples/` contains render fixtures and local HTML/CSS/JS content used by sample flows.

`scripts/` contains reproducible run flows and smoke checks.

---

## Requirements

- C++17 compiler (Clang / GCC / MSVC)
- `cmake >= 3.16`
- `python3` for helper scripts that run local HTTP fixtures
- `OpenSSL` is optional; engine still compiles when available and links when found

---

## Build

### CMake path (preferred)

```bash
cmake -S . -B build
cmake --build build
```

The target is:

```text
from_scratch_browser
```

If `src/main.cpp` is missing in your workspace snapshot, the CMake setup still supports bootstrap build behavior for early validation.

### Direct build fallback (scripts only)

`scripts/run_sample.sh` and `scripts/smoke_test.sh` can still run in environments without a healthy CMake install by directly compiling a fallback source set when needed.

---

## Run

### Basic run

```bash
./build/from_scratch_browser https://example.com output.ppm 1280 720
```

If you are on multi-config generators, the binary is usually under:

```text
build/Debug/from_scratch_browser
build/Release/from_scratch_browser
```

### Scripted runs

```bash
./scripts/run_sample.sh
```

This generates:

- `out.ppm` (HTTP)
- `out_local_path.ppm` (local path input)
- `out_file.ppm` (file URL input)

```bash
./scripts/smoke_test.sh
```

This executes smoke and feature validation and writes fixture artifacts:

- `smoke_out.ppm`
- `smoke_feature_out.ppm`
- `smoke_feature_local_out.ppm`

---

## Inspecting Visual Output

All outputs are `.ppm`, so you can inspect them in an image viewer that supports raw PPM, or convert:

```bash
python3 - <<'PY'
from PIL import Image
img = Image.open("out.ppm")
img.save("out.png")
PY
```

`Pillow` is optional but useful for quick visual confirmation.

---

## Development Rhythm

The repo is designed for iterative, phase-by-phase growth.

Recommended work loop:

- Change a small subset of parser/layout/render behavior.
- Run a focused sample:
  - `./scripts/run_sample.sh`
  - `./scripts/smoke_test.sh`
- Inspect `.ppm` outputs.
- Commit or continue only when the relevant assertion set remains stable.

This project naturally rewards tight cycles and small diff footprints.

---

## What to Expect in Daily Engineering

When extending Vibrowser, every addition should map to one of these questions:

1. Which stage changes (fetch, parse, CSS, layout, render)?
2. Can I validate the change with one fixture and one command?
3. Is the behavior deterministic across runs?
4. Did I keep the implementation in the current style boundary?

If the answer is no for #3 or #4, the change usually gets queued as follow-up engineering debt until a reproducible strategy exists.

---

## Testing Strategy

- Compile and link all tests with CMake
- Use sample fixtures to exercise full pipeline behaviors
- Use smoke tests as guardrails for style + rendering features
- Keep regressions local and small; avoid broad refactors before evidence

---

## Limitations (By Design Right Now)

- Not a full web platform.
- Not a complete JS engine.
- Not a security-hardened production browser.
- Not optimized for modern web-scale features.

The implementation intentionally focuses on a constrained subset so behavior stays tractable and educational.

---

## Philosophy

Vibrowser is built for people who want to see the full stack without pretending complexity is gone.

You can:

- trace behavior from input to output
- add features in small increments
- learn from failures quickly
- build confidence from artifacts instead of guessing

That is the core rhythm of this repo.

---

## Helpful Root Utilities

- `rebuild_and_launch_clever_browser.sh`
- `run_clever_browser_build_and_run.sh`

These are convenience scripts for local launch and rebuild cycles.

---

## Future Work Roadmap

Likely next targets once current behaviors are stable:

- richer selector coverage for production-like CSS edge cases
- broader DOM and style property coverage
- more robust networking edge-case handling
- clearer diagnostics for parser recovery modes
- better test organization for large fixture families
- output viewer tooling for easier visual diffing

This roadmap can change as feature debt becomes measurable.

---

## Contributing

If you are adding parser/selector/render work:

- keep changes tightly scoped to one behavior cluster
- update fixtures in `examples/` to encode intent
- wire verification into existing scripts whenever feasible

No external workflow tooling required to get started; CMake + scripts are enough.

---

## License and Distribution

Project metadata and legal metadata are handled in repository root docs.  
Check the corresponding project files for licensing and usage details before packaging or redistributing.

---

Welcome to Vibrowser.  
Build it loud, keep it explicit, and let output files tell you the truth.
