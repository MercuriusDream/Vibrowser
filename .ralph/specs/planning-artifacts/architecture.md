---
stepsCompleted: [1, 2, 3, 4, 5, 6, 7, 8]
inputDocuments:
  - _bmad-output/planning-artifacts/product-brief-vibrowser-2026-02-23.md
  - _bmad-output/planning-artifacts/prd.md
  - _bmad-output/planning-artifacts/ux-design-specification.md
  - _bmad-output/planning-artifacts/research/technical-vibrowser-research-2026-02-22.md
  - _bmad-output/planning-artifacts/research/domain-web-browser-engine-research-2026-02-22.md
  - _bmad-output/planning-artifacts/research/market-vibrowser-research-2026-02-23.md
  - README.md
  - docs/browser_engine_mvp_backlog.md
  - docs/browser_engine_mvp_roadmap.md
workflowType: 'architecture'
project_name: 'vibrowser'
user_name: 'BMad'
date: '2026-02-23'
lastStep: 8
status: 'complete'
completedAt: '2026-02-23'
---

# Architecture Decision Document

## Project Context Analysis

### Requirements Overview

**Functional Requirements:**

- Navigation and lifecycle control (`run`, URL/path input, retry, cancel, history, stage visibility).
- Deterministic fetch → parse → CSS → layout → render pipeline.
- Mutation-safe DOM parsing/modeling and selector handling.
- CSS parsing, cascade, computed-style behavior, and style consistency diagnostics.
- Layout and rendering stability with predictable geometry output.
- Runtime interaction via limited JavaScript APIs and event input updates.
- Network and resource controls with deterministic request lifecycle, caching, redirects, and observability.
- Diagnostics-rich operations, structured lifecycle traces, milestone-based quality evidence.

**Non-Functional Requirements:**

- P95 baseline render under 2.5 seconds.
- Measurable reliability gates: high fixture pass rate and controlled regression windows.
- Security and privacy controls: explicit consent for telemetry, strict URL/network handling boundaries.
- Deterministic re-renders, recovery clarity, and reproducibility for contributor workflows.
- Scalable architecture with incremental growth while preserving module replacement points.

### Scale & Complexity

- Primary domain: desktop application + CLI toolchain for developer-focused browser-engine infrastructure.
- Domain complexity: **medium**, with high technical depth in parser/style/layout/render interplay.
- Core architecture shape: modular C++17 engine with optional GUI shell and explicit stage/state transitions.

### Technical Constraints & Dependencies

- Current canonical stack is C++17 + CMake.
- Existing module layout is already split by concern (`browser`, `html`, `css`, `js`, `layout`, `render`, `net`, `core`).
- Build currently targets `from_scratch_browser` executable.
- OpenSSL support should remain optional and detected via CMake.
- Existing roadmap calls out progression across milestones with explicit dependencies and conservative compatibility expansion.

### Cross-Cutting Concerns Identified

- Deterministic lifecycle transitions with consistent diagnostics.
- Testability for each stage of the pipeline.
- Privacy and boundaries for runtime/network/observability behavior.
- Stable interfaces so modules can swap in future (e.g., advanced parser or JS runtime components).
- Contributor onboarding quality: predictable structure and narrow module contracts.

## Starter Template Evaluation

### Primary Technology Domain

Desktop-native C++ application with CLI-first compatibility and optional GUI shell.

### Starter Options Considered

- Keep existing C++ + CMake repository scaffold.
- Migrate to another desktop framework starter (`Qt`, `wxWidgets`, `Electron`, etc.).
- Introduce custom starter for shell-first browser tooling.

### Selected Starter Foundation

**Chosen:** existing C++/CMake module-first foundation already in repo.

**Rationale:**

- Aligns with current source layout and existing developer investment.
- avoids adding unrelated app framework opinion and keeps deterministic build behavior.
- preserves ability to introduce shell or GUI adapters behind stable module boundaries.
- minimizes risk and startup cost while enabling milestone-driven expansion.

### Initialization Commands

```bash
cmake -S . -B build
cmake --build build
./build/from_scratch_browser --help
```

### Architectural Decisions Provided by Starter Foundation

- **Language & Runtime:** C++17, single native binary, CMake-driven build.
- **Build Tooling:** CMake source globs with explicit module directories.
- **Testing Baseline:** extend toward CTest + Catch2/GoogleTest when implementation begins.
- **Code Organization:** include-driven modular boundaries already present.
- **Development Workflow:** CLI-first bootstrap path, with GUI path added through engine surface adapters.

## Core Architectural Decisions

### Decision Priority Analysis

**Critical Decisions (Block Implementation):**

1. Preserve and formalize a single-process, modular engine core around deterministic stage state machine.
2. Standardize pipeline interfaces between browser orchestration, parser, style, layout, render, and diagnostics.
3. Normalize lifecycle + request states before adding feature breadth.
4. Build explicit security/privacy boundaries for network and script behavior from milestone one.

**Important Decisions (Shape Architecture):**

1. Introduce an explicit engine facade (`BrowserEngine`) and page/session state model.
2. Add diagnostic bus for stage-level events and failure categories.
3. Isolate rendering outputs through a render output contract for GUI and file/image sinks.
4. Create feature-gated architecture paths for QuickJS and external adapters.

**Deferred Decisions (Post-MVP):**

1. Full multi-process isolation.
2. Full media/WebRTC/WebGPU stack.
3. Deep extension ecosystem and multi-language bindings.

### Data Architecture

- **Storage model:** mostly in-memory with deterministic object ownership; optional on-disk session/history cache in JSON/flat file for contributor UX and reproducibility.
- **Configuration model:** environment + config object with immutable startup profile and mutable runtime flags.
- **Caching model:** request cache keyed by normalized URL + request metadata; cache policy defaults to conservative conservative stale/revalidate strategy.
- **Validation model:** schemaed structures for navigation input, network responses, and diagnostics payloads to keep module contracts explicit.

### Authentication & Security

- No user-auth model is required for MVP; security is boundary-driven:
  - deny-list dangerous schemes and enforce scheme/path hardening.
  - enforce deterministic redirect policy (max depth + failure reason classification).
  - same-origin sensitive behavior must be explicit and logged.
- Script/network runtime calls must be privilege-gated.
- Telemetry and reporting are opt-in and explicit by configuration.

### API & Communication Patterns

- No public web API in MVP.
- Internal communication is contract-first and message-driven:
  - Engine lifecycle events (`loading`, `parsing`, `styling`, `layout`, `paint`, `error`, `recovered`).
  - Request/response objects for network jobs.
  - Event queue for GUI/user input and JS dispatch.
- All cross-module communication uses explicit structs, enums, and status enums (no ad-hoc strings in public boundaries).

### Frontend Architecture

- Primary interface remains CLI with structured output for deterministic diagnostics.
- Optional GUI shell consumes the same engine contracts and render sink abstractions.
- Lifecycle panels are UI-facing projections of engine state and diagnostics, not separate logic engines.

### Infrastructure & Deployment

- CMake + C++17 binary as primary pipeline.
- Optional GUI backends (SDL2/GLFW path) behind compile-time abstraction.
- Packaging is out of MVP scope; prioritize reproducible build and script-driven smoke test entrypoints.

### Decision Impact Analysis

**Implementation Sequence:**

1. Formalize engine + state model.
2. Add request/response + diagnostics contracts.
3. Add module boundaries with compile-safe interfaces.
4. Add render sink abstraction.
5. Introduce async scheduler and caching behavior.
6. Extend CLI and GUI surfaces against the same contracts.

**Cross-Component Dependencies:**

- Network module depends on URL/input normalization and diagnostics.
- DOM/CSS/Layout/Render depend on deterministic lifecycle transitions.
- JS bridge depends on DOM/mutation and event models.
- Diagnostics depend on all other modules for complete stage-level observability.

## Implementation Patterns & Consistency Rules

### Pattern Categories Defined

- 30+ potential conflict points were evaluated across naming, interfaces, state transitions, and error semantics.
- Architecture mandates are designed to prevent module divergence in future AI-generated contributions.

### Naming Patterns

**Database/State Naming:**

- Use `snake_case` for serialized keys, `PascalCase` for enums and class names.
- Use singular nouns for domain objects (`PageState`, `NavigationRecord`, `RenderFrame`), collection names plural when represented as arrays.
- Error codes must use `CamelCase` enum values only where protocol parity exists.

**API Naming:**

- Keep stable, public-ish function names in `verbNoun` style:
  - `open_url`, `schedule_fetch`, `build_style_tree`, `render_frame`.
- Keep file names aligned with module and type names.

### Structure Patterns

- **Feature grouping by module, not by feature state:**
  - `browser/engine`, `browser/net`, `browser/html`, `browser/css`, `browser/js`, `browser/layout`, `browser/render`.
- **Tests:** place under `tests/unit` and `tests/integration` by module.
- **Adapters:** UI/network/runtime adapters go in dedicated subdirectories under their owning module.

### Format Patterns

- Error payload:
  - `code` (enum), `message`, `source`, `severity`, `stage`, `context`.
- Diagnostics payload:
  - `timestamp`, `event`, `severity`, `correlation_id`, `module`, `metadata`.
- State snapshots:
  - stable JSON-like serializable fields, avoiding raw pointer leakage.

### Communication Patterns

- Engine events use an explicit event bus (`emit`, queue, consume by consumer groups).
- Task lifecycle uses status enums: `queued -> running -> completed -> failed/canceled`.
- Mutation propagation follows deterministic dirty marks.

### Process Patterns

- Loading behavior is never silent; each stage must emit status and duration.
- Errors must map to stage + actionable recovery path.
- Recovery actions are limited to supported states and validated before replay.

### Enforcement

**All implementation contributors MUST:**

- follow module interfaces defined in headers first;
- preserve stage order and event semantics;
- keep module boundaries stable for public headers;
- include deterministic diagnostics for lifecycle transitions.

**Pattern Verification:**

- CI review checks should include API drift and naming consistency for changed headers.
- Stage trace snapshots should be part of regression tests for core paths.

### Pattern Examples

**Good Example:**

- `src/net/fetch_scheduler` owns request queue state and emits `FetchStarted`, `FetchProgress`, `FetchFinished` events consumed by browser engine.

**Anti-Pattern:**

- direct module-to-module calls from `js` to `render` without stage mediation or diagnostics context.

## Project Structure & Boundaries

### Complete Project Directory Structure

```text
from_scratch_browser/
├── CMakeLists.txt
├── README.md
├── README_ARCHITECTURE.md (new, optional)
├── docs/
│   ├── browser_engine_mvp_backlog.md
│   ├── browser_engine_mvp_roadmap.md
│   └── architecture.md
├── include/
│   ├── browser/
│   │   ├── browser.h
│   │   ├── core/
│   │   │   ├── config.h
│   │   │   ├── diagnostics.h
│   │   │   └── lifecycle.h
│   │   ├── engine/
│   │   │   ├── engine.h
│   │   │   ├── page_state.h
│   │   │   ├── navigation.h
│   │   │   └── scheduler.h
│   │   ├── html/
│   │   │   ├── dom.h
│   │   │   └── html_parser.h
│   │   ├── css/
│   │   │   ├── css_parser.h
│   │   │   └── style_tree.h
│   │   ├── js/
│   │   │   ├── runtime.h
│   │   │   ├── bindings.h
│   │   │   └── bridge.h
│   │   ├── layout/
│   │   │   ├── layout_engine.h
│   │   │   ├── layout_tree.h
│   │   │   └── geometry.h
│   │   ├── render/
│   │   │   ├── canvas.h
│   │   │   ├── renderer.h
│   │   │   ├── scene.h
│   │   │   └── sink.h
│   │   ├── net/
│   │   │   ├── http_client.h
│   │   │   ├── url.h
│   │   │   ├── request.h
│   │   │   ├── response.h
│   │   │   └── fetch_scheduler.h
│   │   ├── events/
│   │   │   ├── event.h
│   │   │   ├── event_queue.h
│   │   │   └── input.h
│   │   ├── ui/
│   │   │   ├── window.h
│   │   │   ├── surface.h
│   │   │   └── chrome.h
│   │   └── security/
│   │       ├── policy.h
│   │       └── origin.h
│   └── common/
│       ├── error.h
│       └── result.h
├── src/
│   ├── main.cpp
│   ├── browser/
│   │   └── browser.cpp
│   ├── core/
│   │   ├── diagnostics.cpp
│   │   └── lifecycle.cpp
│   ├── html/
│   │   └── html_parser.cpp
│   ├── css/
│   │   └── css_parser.cpp
│   ├── js/
│   │   └── runtime.cpp
│   ├── layout/
│   │   └── layout_engine.cpp
│   ├── net/
│   │   ├── http_client.cpp
│   │   ├── url.cpp
│   │   ├── request.cpp
│   │   └── fetch_scheduler.cpp
│   ├── render/
│   │   └── renderer.cpp
│   ├── events/
│   │   └── event_queue.cpp
│   ├── ui/
│   │   ├── window.cpp
│   │   └── surface.cpp
│   └── utils/
│       └── ...
├── scripts/
│   ├── run_sample.sh
│   ├── smoke_test.sh
│   └── (future) smoke_integration.sh
├── examples/
│   └── sample.html
└── third_party/
    └── quickjs/ (deferred)
```

### Architectural Boundaries

**API Boundaries:**

- `core` owns configuration and diagnostic model.
- `engine` owns lifecycle orchestration and state transitions.
- `net` owns requests, responses, scheduler, redirects.
- `html/css/layout/render` owns dataflow and transforms of document representation.

**Component Boundaries:**

- Browser modules should not read each other’s internals directly.
- Each module only consumes contracts from peers through explicit headers and structs.

**Service Boundaries:**

- UI is a consumer of engine output and diagnostic events.
- JS module consumes DOM/document contracts and emits mutations via engine-safe interfaces.

**Data Boundaries:**

- DOM/Style/Layout/Render state flows one direction per cycle (`parse -> style -> layout -> render`).
- Mutations in DOM trigger layout/style recomputation via invalidation rules.

### Requirements to Structure Mapping

**Feature Category Mapping:**

- Navigation/Lifecycle FR1-6 → `src/browser`, `src/core`, `src/net`.
- DOM/Parser FR7-12 → `src/html`.
- Styling FR9-12 → `src/css`.
- Layout/Rendering FR13-17 → `src/layout`, `src/render`.
- JS Interaction FR18-21 → `src/js`, `src/events`.
- Networking FR22-25 → `src/net`.
- Diagnostics/Quality FR26-30 → `src/core` plus `scripts`.

### Cross-Cutting Concerns Mapping

**Observability:**

- Diagnostics bus at `core` and `engine` boundaries, emitted into CLI/UI.

**Security:**

- URL/request/input and policy checks in `net` and `security`.

**Compatibility Growth:**

- Optional adapter layer in `js` and `render` to support external engine replacements.

### Integration Points

**Internal Communication:**

- Event queue + lifecycle event stream consumed by CLI/UI and logs.

**External Integrations:**

- OpenSSL (optional), optional GUI backend (SDL2/GLFW), optional QuickJS in later phase.

**Data Flow:**

- URL/request → fetch → parse → css/style → layout → render → diagnostics event → optional GUI/ppm output.

### File Organization Patterns

- Config: root `README`, `docs`, and root-level scripts.
- Source: module root plus module-specific subfolders.
- Tests: `tests/unit` and `tests/integration` once harness is introduced.

### Development Workflow Integration

- Dev shell runs with existing scripts and CMake build.
- New milestone changes include update of docs with acceptance criteria and smoke coverage.

## Architecture Validation Results

### Coherence Validation ✅

- Technology stack and module boundaries are internally consistent with current repo and PRD requirements.
- Core decision areas (pipeline, state, diagnostics, networking) are aligned with milestone-driven roadmap.
- Naming, contracts, and directory responsibilities are coherent and enforceable.

### Requirements Coverage Validation ✅

- All PRD functional categories are mapped to modules in this architecture.
- Non-functional requirements are captured as first-class design constraints (performance, reliability, privacy).
- Cross-cutting concerns (determinism, observability, security-by-default) are explicitly represented.

### Implementation Readiness Validation ✅

- Decision completeness: high confidence for MVP and Milestone 1-2 implementation sequencing.
- Structure completeness: all required module families and adapters are represented.
- Pattern completeness: consistency rules cover naming, eventing, error, and state behavior.

### Gap Analysis Results

- **Critical Gaps:** none that block implementation of the stated MVP scope.
- **Important Gaps:** full test harness details, concrete telemetry schema, and concrete quickjs integration path remain to be finalized in sprint-level tasks.
- **Nice-to-Have Gaps:** richer extension/plugin interfaces and full multi-process model.

### Validation Issues Addressed

- Added explicit staging and boundary rules to prevent module drift.
- Standardized diagnostics and state semantics for implementation contributors.
- Documented deferred post-MVP decisions to avoid scope inflation.

### Architecture Readiness Checklist

**✅ Requirements Analysis**

- [x] Project context analyzed
- [x] Scale and complexity assessed
- [x] Technical constraints identified
- [x] Cross-cutting concerns mapped

**✅ Architectural Decisions**

- [x] Critical decisions documented
- [x] Technology stack specified
- [x] Integration patterns defined
- [x] Performance and reliability considerations captured

**✅ Implementation Patterns**

- [x] Naming conventions established
- [x] Structure patterns defined
- [x] Communication patterns specified
- [x] Process patterns documented

**✅ Project Structure**

- [x] Directory structure defined
- [x] Component boundaries established
- [x] Integration points mapped
- [x] Requirements-to-structure mapping complete

### Architecture Readiness Assessment

- **Overall Status:** READY FOR IMPLEMENTATION
- **Confidence Level:** High for MVP (staging and boundaries), Medium for full standards coverage expansion.

**Key Strengths:**

- Strong fit with existing module layout and pipeline behavior.
- Deterministic architecture directly aligned to PRD and UX requirement of transparent lifecycle.
- Explicit boundaries reduce ambiguity for AI-generated or multi-contributor implementation.

**Areas for Future Enhancement:**

- Add runtime metrics + budgets in C++ core.
- Add persistent history/session cache formats.
- Add renderer adapters and optional advanced backend swaps.

### Implementation Handoff

- AI contributors must adhere to documented module contracts and status conventions.
- Primary implementation priority: implement `engine + diagnostics + fetch + lifecycle state contracts` before feature additions.
- Use architecture section map when writing/accepting implementation plans.

## Completion Summary

The architecture workflow is complete. We now have:

- Full project context analysis.
- Starter evaluation aligned to the existing C++/CMake foundation.
- Documented implementation decisions and critical boundaries.
- Consistency rules to prevent agent-level drift.
- Concrete structure and integration mapping from requirements to modules.
- Validation outcome and implementation-readiness assessment.

You can now move to implementation planning and story-level execution.
