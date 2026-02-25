# Viability Verdict

## ViabilityGateRecord

| label | engine_viability | browser_viability | gate_criteria | missing_critical | blocking_issues | recommended_grade |
|---|---|---|---|---|---|---|
| L2 | `true` | `partial` | Run a constrained deterministic engine and rendering pipeline (lifecycle/stage tracing, parse->style->layout->render), then satisfy spec-facing invariants for DOM, task-loop ordering, fetch/cookie/CORS/CSP, and TLS + transport hardening. | Missing/partial HTML+DOM conformance, no WebIDL generation, incomplete task-microtask model, CORS/CSP enforcement gaps, incomplete cookie/TLS policy enforcement, and no HTTP/2/3 transport path. | MissingCritical IDs: `MC-01`, `MC-02`, `MC-03`, `MC-04`, `MC-05`, `MC-06`, `MC-07`, `MC-08`, `MC-09`, `MC-12`; blocking issues also include `FJNS-06`, `FJNS-11`, `FJNS-14` for runtime security and policy correctness. | `L2` |

## Evidence summary

- Engine baseline is runnable and deterministic by design:
  - `include/browser/core/lifecycle.h:9-19`
  - `src/engine/engine.cpp:28-105`
  - `src/browser/browser.cpp:1448-1515`
  - `src/layout/layout_engine.cpp:151-455`
- Standardization-critical gaps are consistently visible in implementation:
  - `src/html/html_parser.cpp:238-508`
  - `src/js/runtime.cpp:1155-1181`
  - `src/net/http_client.cpp:1146-1196`
  - `src/net/http_client.cpp:285-317`
  - `clever/src/html/tree_builder.cpp:295-788`
  - `clever/src/js/js_dom_bindings.cpp:1001-1381`
  - `clever/src/net/cookie_jar.cpp:31-139`
  - `clever/src/js/js_fetch_bindings.cpp:135-205`
- Governance and planning alignment remains clear:
  - `audit/full-traversal-coverage-ledger.md:1`
  - `audit/wave1/wave1a-from-scratch-architecture.md:1`
  - `audit/wave1/wave1b-clever-architecture.md:1`
  - `audit/wave2/standards-compliance-matrix.md:1` (generated below)
  - `audit/wave6/final/adversarial-verification.md:1`

## Recommended staged target

- `engine_viability`: maintain `true` for constrained deterministic replay and constrained engine-shell behavior.
- `browser_viability`: remains `partial` until Stage 2 and Stage 3 prerequisites are closed.

### Stage 2: spec-facing core
- Parser + DOM + CSS + URL + basic Fetch + request policy gates for safe deterministic documents.
- Add adversarially validated conformance tests for parsing, mutation, queues, and storage edges.

### Stage 3: production-hardened browser shell
- Add transport/state hardening: TLS trust enforcement, robust CORS/CSP enforcement, SameSite/partitioned cookie behavior, cache-control partitioning.
- Complete event-loop scheduling contract (`microtask`/`macrotask`/`RAF`) and security telemetry.

## Recommended grade rationale

`L2` remains appropriate because:
- Deterministic engine substrate is present (`src/engine/engine.cpp`, `src/browser/browser.cpp`, rendering/layout path).
- Security and standards surfaces are partially implemented but still non-browser-grade (`MC-*` entries above).
- Missing policy and runtime guarantees are known and now explicitly enumerated via Wave 6 adversarial review rather than inferred from architecture prose alone.
- Closure path is bounded, staged, and feasible with the current architecture if Stage 2 and Stage 3 gates are executed before claiming a higher grade.
