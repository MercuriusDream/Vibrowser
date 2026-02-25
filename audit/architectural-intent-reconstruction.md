# Architectural Intent Reconstruction

## Executive summary

The repository contains two browser code paths:

- `from_scratch_browser`-style implementation anchored in `src/` and `include/`.
- `clever` implementation anchored in `clever/src/` and `clever/include/`.

Both share project-level planning/docs intent, but evidence indicates intentional separation of fidelity:
- `from_scratch`: deterministic, single-process render pipeline with explicit lifecycle controls.
- `clever`: broader architecture target (WHATWG/Fetch/WebIDL/etc.) with selective subsystem scaffolding.

## FindingRecord (cross-wave reconstruction)

| id | subsystem | scope | status | severity | criticality | evidence | spec_citation | certainty | notes |
|---|---|---|---|---|---|---|---|---|
| AR-01 | from_scratch | `src/`, `include/` | Implemented | Low | Low | `include/browser/core/lifecycle.h:1` `include/browser/core/diagnostics.h:1` | Project architecture intent | High | Explicit lifecycle state machine and diagnostics channels indicate deterministic control model. |
| AR-02 | from_scratch | `src/net`, `include/browser/net/http_client.h:1` | Implemented | Medium | Medium | `include/browser/net/url.h:1` `include/browser/net/http_client.h:1` | Custom URL+HTTP surface + policy hooks | High | URL resolution and HTTP client are custom wrappers; intended to support constrained navigation semantics. |
| AR-03 | from_scratch | `src/html`, `include/browser/html/dom.h:1` | Partial | High | High | `src/html/html_parser.cpp:1` `include/browser/html/dom.h:1` | HTML/DOM minimal model | High | Parser and DOM model are custom; no explicit full-tree construction parity with HTML5 parsing model. |
| AR-04 | from_scratch | `src/css`, `include/browser/css/css_parser.h:1` | Partial | Medium | Medium | `src/css/css_parser.cpp:1` `include/browser/css/css_parser.h:1` | Selector/style subset | High | CSS parsing and selector support are custom and intentionally limited. |
| AR-05 | from_scratch | `include/browser/js/runtime.h:1`, `src/js/runtime.cpp:1` | Partial | Medium | Medium | `src/js/runtime.cpp:1` | JS host boundary mapping | Medium | Runtime exists with host callbacks and timer integration but not full ECMAScript host model. |
| AR-06 | from_scratch | `include/browser/render/renderer.h:1` | Implemented | Medium | Medium | `include/browser/render/renderer.h:1` `src/render` | Renderer + layout loop | High | Contains renderer abstraction for layout/paint orchestration, aligned with stage pipeline intent. |
| AR-07 | clever | `clever/src/url`, `clever/src/net`, `clever/src/dom` | Partial | High | High | `CLEVER_ENGINE_BLUEPRINT.md:1` | Architecture target document | High | Blueprint indicates multi-domain architecture with URL/IPC/Platform/Net/DOM/HTML modules. |
| AR-08 | clever | `clever/src/` runtime entry | Partial | High | High | `README.md:1` `CLEVER_ENGINE_BLUEPRINT.md:1` | Blueprint goal + current module split | High | Subsystems exist but runtime orchestration depth is not yet equivalent across all modules. |
| AR-09 | shared | `_bmad-output/planning-artifacts/` | Implemented | High | High | `_bmad-output/planning-artifacts/architecture.md:1` `_bmad-output/planning-artifacts/prd.md:1` | PRD + architecture artifacts | High | Shared planning defines target browser-shell behavior, security policy posture, and staged maturity. |

## Subsystem intent map

### from_scratch intent

1. Core orchestration
- Create/initialize browser session and state.
- Parse/fetch content, then process through HTML→DOM→CSS→layout/render steps.
- Expose diagnostics and lifecycle boundaries.

2. Data flow intent
- `url` resolution → `http_client` request → HTML parse → CSS parse → DOM build/style attach → render/layout output.
- Diagnostics and cache/policy metadata are threaded for traceability and reproducibility.

### clever intent

1. Multi-module architecture intent
- URL service, IPC/service bridge, platform adapters, networking and media modules.
- DOM/HTML subsystem target for parser/tokenization and document lifecycle alignment.

2. Migration/target intent
- Blueprint explicitly references alignment to broader web platform components (fetch, URL, security policy, CORS/SOP/CSP, HTTP features), i.e., a higher-conformance path than current from-scratch implementation.

## Architectural risk framing

- Deterministic educational path is clearly present and tractable.
- Conformance-critical semantics are partial (DOM/CSS/JS/Network standards), creating a gap between blueprint intent and implementation across both paths.
- Security policy and session/modeling semantics are present in API shape yet shallow in enforcement depth.

## Alignment to standards scope

- `from_scratch` has explicit intent for stage-based rendering and deterministic behavior (suitable for an educational or constrained engine).
- `clever` has explicit intent for broader platform parity but currently reads as a partially realized architecture rather than fully implemented compliance.
