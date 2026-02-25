# Wave1-A: from_scratch_browser Architecture Reconstruction

## Scope
- Code scope: `src/browser/browser.cpp`, `src/engine/engine.cpp`, `src/engine/navigation.cpp`, `src/engine/render_pipeline.cpp`
- Supporting contracts examined for request/diagnostics state semantics: `src/net/http_client.cpp`, `src/core/lifecycle.cpp`, `src/core/diagnostics.cpp`, related `include/browser/*` headers
- Focus: lifecycle states, run pipeline, request lifecycle, diagnostics/events

## Control Flow Reconstruction
1. CLI entrypoint creates `BrowserEngine` and calls `navigate(input, render_opts)` (`src/main.cpp:171-177`).
2. `BrowserEngine::navigate` resets session/cancel flag, normalizes input metadata, emits a navigation diagnostic, then delegates execution to `browser::app::run` with stage and cancellation callbacks (`src/engine/engine.cpp:59-97`).
3. `browser::app::run` executes a fixed synchronous pipeline:
   - Fetch (`load_text_resource_cached`)
   - Parse (`parse_html` + base URL resolution + script execution)
   - Styling (`collect_style_text` + `parse_css`)
   - Layout (`layout_document`)
   - Rendering (`render_to_canvas` + `write_ppm`)
   (`src/browser/browser.cpp:1448-1506`).
4. Stage entry is surfaced back into engine via callback mapping (`src/engine/engine.cpp:13-22`, `89-91`), updating lifecycle state and trace.
5. Engine finalizes to `Complete`, `Error`, or `Cancelled` after `run` returns (`src/engine/engine.cpp:99-105`).

## Data Flow Reconstruction
- Session state is held in `SessionInfo` (`navigation`, `stage`, `diagnostics`, `trace`) (`include/browser/engine/engine.h:13-18`).
- Input normalization/canonicalization is represented in `NavigationInput` (`src/engine/navigation.cpp:71-117`), but `run` is invoked with raw `input` (`src/engine/engine.cpp:97`).
- Runtime resource loading uses `TextLoadResult` and per-run `TextResourceCache` keyed by canonical URL (`src/browser/browser.cpp:27-37`, `702-714`).
- Warning diagnostics in `browser::app::run` are accumulated as plain strings and appended to `RunResult.message` (`src/browser/browser.cpp:1460-1515`).

## Lifecycle State Reconstruction
- Lifecycle enum supports `Idle -> Fetching -> Parsing -> Styling -> Layout -> Rendering -> Complete|Error|Cancelled` (`include/browser/core/lifecycle.h:9-19`).
- Every engine transition updates `session_.stage`, records a `LifecycleTrace` timing entry, and emits a diagnostic (`src/engine/engine.cpp:28-38`; `src/core/lifecycle.cpp:22-35`).
- Cancellation is cooperative via atomic flag and stage-boundary checks (`src/engine/engine.cpp:52-57`, `92-94`; `src/browser/browser.cpp:1450`, `1464`, `1480`, `1490`, `1498`).

## Request Lifecycle Reconstruction
- Active document/resource fetch path in `run` calls `browser::net::fetch` directly (`src/browser/browser.cpp:661`).
- `fetch` provides redirect loop handling, timeout detection, and `final_url`/duration metadata (`src/net/http_client.cpp:957-1013`).
- Rich request contract exists separately (`RequestTransaction`, `RequestStage`, observer callback, `fetch_with_contract`) (`src/net/http_client.cpp:1023-1078`) but is not wired into `run`/`BrowserEngine`.

## Diagnostics and Event Reconstruction
- Engine stores structured diagnostic events (`timestamp`, `severity`, `module`, `stage`, `message`) in session vector (`src/engine/engine.cpp:40-50`; `include/browser/core/diagnostics.h:17-24`).
- In practice, engine-emitted diagnostics use `Severity::Info` only (`src/engine/engine.cpp:37`, `55`, `79`).
- Pipeline warnings/errors from script/style/fetch side paths are flattened into `RunResult.message`, not emitted as structured events (`src/browser/browser.cpp:1511-1515`).
- Event-bus style primitives (`DiagnosticEmitter`, observers; request transaction observer) exist but are not integrated into engine orchestration (`include/browser/core/diagnostics.h:32-57`, `include/browser/net/http_client.h:61-69`).

## Findings Matrix
| id | path:line | status | spec_citation | certainty | impact | scope_overlap |
|---|---|---|---|---|---|---|
| W1A-LS-01 | include/browser/core/lifecycle.h:9 | Implemented | PRD FR2/FR6 (`_bmad-output/planning-artifacts/prd.md:298,302`); Story 1.2/1.5 (`_bmad-output/planning-artifacts/stories.md:175-186,214-225`) | High | High | lifecycle_states |
| W1A-LS-02 | src/engine/engine.cpp:28 | Implemented | Story 1.2 AC requires visible stage events (`_bmad-output/planning-artifacts/stories.md:183-186`) | High | High | lifecycle_states,diagnostics_events |
| W1A-LS-03 | src/core/lifecycle.cpp:22 | Implemented | Story 1.5 timing reproducibility (`_bmad-output/planning-artifacts/stories.md:222-225`); PRD FR6 (`_bmad-output/planning-artifacts/prd.md:302`) | High | Medium | lifecycle_states |
| W1A-LS-04 | src/browser/browser.cpp:1450 | Partial | Story 1.4 in-flight cancel expectation (`_bmad-output/planning-artifacts/stories.md:209-212`) | High | High | lifecycle_states,run_pipeline,request_lifecycle |
| W1A-RP-01 | src/browser/browser.cpp:1448 | Implemented | Deterministic pipeline requirement (`_bmad-output/planning-artifacts/architecture.md:30`; PRD Executive Summary pipeline in `_bmad-output/planning-artifacts/prd.md:43`) | High | High | run_pipeline |
| W1A-RP-02 | src/browser/browser.cpp:746 | Implemented | FR24 deterministic linked CSS/resource handling (`_bmad-output/planning-artifacts/prd.md:332`; Story 5.3 in `_bmad-output/planning-artifacts/stories.md:454-465`) | Medium | Medium | run_pipeline,request_lifecycle |
| W1A-RP-03 | include/browser/engine/engine.h:20 | Missing | FR16 dual output path (headless + shell) (`_bmad-output/planning-artifacts/prd.md:318`) | High | Medium | run_pipeline |
| W1A-RP-04 | src/engine/render_pipeline.cpp:31 | Partial | FR21 rerender after runtime mutation (`_bmad-output/planning-artifacts/prd.md:326`) | Medium | Medium | run_pipeline,lifecycle_states |
| W1A-RL-01 | src/engine/navigation.cpp:44 | Implemented | FR1 and Story 1.1 input forms and normalized metadata (`_bmad-output/planning-artifacts/prd.md:297`; `_bmad-output/planning-artifacts/stories.md:170-173`) | High | High | request_lifecycle,lifecycle_states |
| W1A-RL-02 | src/engine/engine.cpp:97 | Partial | Story 1.1 normalized input determinism (`_bmad-output/planning-artifacts/stories.md:172-173`); Architecture validation model (`_bmad-output/planning-artifacts/architecture.md:128`) | Medium | Medium | request_lifecycle,run_pipeline |
| W1A-RL-03 | src/browser/browser.cpp:621 | Implemented | FR22 request/response handling + FR24 linked resources (`_bmad-output/planning-artifacts/prd.md:330,332`) | High | High | request_lifecycle |
| W1A-RL-04 | src/net/http_client.cpp:957 | Implemented | FR25 redirects/boundaries (`_bmad-output/planning-artifacts/prd.md:333`); Story 5.4 (`_bmad-output/planning-artifacts/stories.md:467-478`) | High | High | request_lifecycle |
| W1A-RL-05 | src/net/http_client.cpp:1042 | Implemented | Story 5.1 request dispatch/receive/complete contract (`_bmad-output/planning-artifacts/stories.md:428-439`) | High | Medium | request_lifecycle |
| W1A-RL-06 | src/browser/browser.cpp:661 | Missing | Story 5.1 requires lifecycle visibility in diagnostics for request contract (`_bmad-output/planning-artifacts/stories.md:437-439`) | High | High | request_lifecycle,diagnostics_events |
| W1A-RL-07 | src/net/http_client.cpp:1088 | Missing | FR23 cache policy + Story 5.2 cache-hit diagnostics (`_bmad-output/planning-artifacts/prd.md:331`; `_bmad-output/planning-artifacts/stories.md:441-453`) | High | High | request_lifecycle,diagnostics_events |
| W1A-RL-08 | src/net/http_client.cpp:1146 | Missing | FR25/Story 5.4 policy enforcement in handling path (`_bmad-output/planning-artifacts/prd.md:333`; `_bmad-output/planning-artifacts/stories.md:475-478`) | High | High | request_lifecycle |
| W1A-RL-09 | src/browser/browser.cpp:702 | Partial | FR23 reuse via cache policy (`_bmad-output/planning-artifacts/prd.md:331`) | High | Medium | request_lifecycle,run_pipeline |
| W1A-DE-01 | src/engine/engine.cpp:40 | Implemented | Story 6.1 structured diagnostic fields (`_bmad-output/planning-artifacts/stories.md:495-506`) | High | Medium | diagnostics_events,lifecycle_states |
| W1A-DE-02 | src/engine/engine.cpp:37 | Partial | FR26 + Story 6.1 severity/stage context (`_bmad-output/planning-artifacts/prd.md:337`; `_bmad-output/planning-artifacts/stories.md:503-506`) | High | High | diagnostics_events |
| W1A-DE-03 | include/browser/core/diagnostics.h:23 | Missing | Story 6.1 correlation metadata requirement (`_bmad-output/planning-artifacts/stories.md:505`) | High | High | diagnostics_events |
| W1A-DE-04 | include/browser/core/diagnostics.h:32 | Missing | Architecture explicit event bus requirement (`_bmad-output/planning-artifacts/architecture.md:213-214`) | Medium | High | diagnostics_events,request_lifecycle,lifecycle_states |

## Notes
- `Incorrect`/`Undefined` status was not assigned in this slice because observed gaps mapped cleanly to `Partial` or `Missing` relative to available explicit spec language.
