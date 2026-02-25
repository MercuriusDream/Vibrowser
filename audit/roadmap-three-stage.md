# Roadmap: Three-Stage Browser Completion Program

## RoadmapMilestoneRecord

| milestone | goal | minimal_primitives | deps | verification_scenario | risk | effort |
|---|---|---|---|---|---|---|
| Stage 1 — Minimal Educational Browser | Deliver deterministic, reproducible parse→style→layout→render for constrained inputs | `include/browser/core/lifecycle.h`, `src/engine/engine.cpp`, `src/browser/browser.cpp`, `src/html/html_parser.cpp`, `src/css/css_parser.cpp`, `src/layout/layout_engine.cpp`, `src/render/renderer.cpp` | Existing from_scratch modules + test corpus + trace assertions | Replay fixed seed corpus and assert deterministic stage order/timing fields across both `src/browser/browser.cpp` and `src/engine/engine.cpp` traces | Deterministic pipeline assumptions may diverge outside curated subset | Medium |
| Stage 2 — Spec-Conforming Engine Path | Align parser/DOM/CSS/runtime/network APIs to constrained browser-grade behavior | `src/net/url.cpp`, `src/html/html_parser.cpp`, `src/css/css_parser.*`, `src/js/runtime.*`, `src/net/http_client.*`, `audit/missing-components-criticality-map.md:1`, `audit/standards-compliance-matrix.md` | Stage 1 + `clever/src/{url,dom,html,css,js,net}` for cross-check and parity scaffolding | Differential corpus for parser/tree, DOM mutation, selector cascade, fetch redirects/cookies, and deterministic task ordering including timers/microtasks/RAF | Unmodeled spec gaps and selector/parser explosion can delay convergence | High |
| Stage 3 — Production-Hardened Browser | Add transport/security hardening for external untrusted content | TLS policy enforcement (`SSL` verify + failure telemetry), CORS/CSP/SOP policy engines, cookie partitioning, protocol maturity (H2/H3), and event-loop invariants | Stage 2 + `audit/roadmap-three-stage.md` + hardening artifacts (`audit/viability-verdict.md`) | End-to-end scenarios: redirect chain + cross-origin request blocking, CSRF/cookie mismatch, mixed-content/TLS failure, timer/event ordering stress, and shared-cache isolation | Security regressions and cross-origin policy bypass risk due to partial enforcement | Very High |

## Stage mapping evidence

- Stage 1 baseline evidence:
  - `audit/wave1/wave1a-from-scratch-architecture.md`
  - `audit/full-traversal-coverage-ledger.md`
- Stage 2 evidence:
  - `audit/wave2/standards-compliance-matrix.md`
  - `audit/wave3/rendering-findings.md`
  - `audit/wave4/network-security-findings.md`
- Stage 3 evidence:
  - `audit/wave6/final/adversarial-verification.md`
  - `audit/missing-components-criticality-map.md`
  - `audit/viability-verdict.md`

## Recommended next 8-week sequencing

1. Close parser-tree and DOM/API gaps (MC-01, MC-02, MC-03) with explicit Wave3 test harnesses from both engines.
2. Add host scheduling and task-loop semantics (MC-05, MC-06, FJNS-05, FJNS-14) before enabling any deeper CORS/CSP assertions.
3. Implement policy stack hardening (MC-08, MC-11, MC-09, MC-13) and network verification scenarios.
4. Validate Stage 2 with adversarially reviewed evidence and gate transition from `L2` to `L3` only if all high-impact blockers are resolved.
