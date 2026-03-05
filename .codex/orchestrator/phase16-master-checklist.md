# Vibrowser Phase 16 Master Checklist

This document is the long-horizon master checklist for completing the Vibrowser modernization program through Phase 16.

## Usage Contract

- The planner must read this file every cycle.
- The planner must align near-term workload choices against this long-range sequence.
- The planner must prefer workloads that unlock downstream phases instead of only maximizing local feature count.
- The planner must keep each cycle atomic even when the master checklist spans years of work.

## Completion Standard

- A phase is not complete because code exists.
- A phase is complete only when implementation, tests, traceability, and real-site relevance are all satisfied.
- Every item below should eventually resolve into concrete workloads in `ultra-long-horizon-workload-map.md` and then into cycle-local planner output.

## Phase 0 — Program Control and Architectural Grounding

- [ ] P0-001 Establish `.codex/codex-estate.md` as canonical and keep `.claude/claude-estate.md` shadow-synced.
- [ ] P0-002 Maintain explicit orchestration state in `.codex/orchestrator/state/`.
- [ ] P0-003 Keep planner/integrator/verifier/fixer prompts schema-driven.
- [ ] P0-004 Keep every cycle bounded to 6 disjoint workloads.
- [ ] P0-005 Preserve mtime-based ledger divergence handling notes when shadow/canonical copies drift.
- [ ] P0-006 Keep build/test/bench/smoke scripts stable enough for machine invocation.
- [ ] P0-007 Document ownership boundaries by subsystem.
- [ ] P0-008 Keep orchestration runtime independent from app runtime.
- [ ] P0-009 Maintain launchd/tmux/watch tooling as first-class infra.
- [ ] P0-010 Preserve operator observability without requiring manual intervention.

## Phase 1 — Runtime Spine and Safety

- [ ] P1-001 Remove all JS thread-affinity violations.
- [ ] P1-002 Correct event-loop delayed-task wake semantics.
- [ ] P1-003 Split task queues by source and priority.
- [ ] P1-004 Make timer semantics browser-like rather than host-flush-based.
- [ ] P1-005 Move observer delivery to correct checkpoint boundaries.
- [ ] P1-006 Remove sleep-based polling where deterministic scheduling is required.
- [ ] P1-007 Add starvation, lag, and long-task traces.
- [ ] P1-008 Add runtime unit tests for ordering guarantees.
- [ ] P1-009 Bound IPC message sizes and enforce backpressure.
- [ ] P1-010 Eliminate lifecycle leaks in worker, websocket, and observer state.

## Phase 2 — Network Engine Modernization

- [ ] P2-001 Fix HTTP/2 deadlocks and protocol incompleteness.
- [ ] P2-002 Move fetch/XHR semantics toward truly async behavior.
- [ ] P2-003 Introduce request prioritization by resource class.
- [ ] P2-004 Add DNS caching and connect-path latency reductions.
- [ ] P2-005 Introduce streaming response parsing.
- [ ] P2-006 Improve cache key normalization and correctness.
- [ ] P2-007 Improve cookie host-only/path/domain semantics.
- [ ] P2-008 Add cancellation propagation from JS API to network stack.
- [ ] P2-009 Add connection reuse policy instrumentation.
- [ ] P2-010 Build real-site networking regression corpus.

## Phase 3 — HTML, DOM, and Parsing Integrity

- [ ] P3-001 Move toward incremental HTML parsing.
- [ ] P3-002 Separate parser pause/resume from top-level render sync points.
- [ ] P3-003 Reduce full-subtree helper scans for common DOM operations.
- [ ] P3-004 Improve DOM mutation cost model and ownership rules.
- [ ] P3-005 Preserve source-node integrity during layout-related transformations.
- [ ] P3-006 Improve DOM query performance on large trees.
- [ ] P3-007 Add deterministic parser recovery tests.
- [ ] P3-008 Expand malformed HTML recovery coverage.
- [ ] P3-009 Build composed-tree aware planning for future Shadow DOM depth.
- [ ] P3-010 Connect DOM invalidation to style/layout incrementality.

## Phase 4 — CSS Parsing and Selector Engine

- [ ] P4-001 Precompute selector specificity at parse time.
- [ ] P4-002 Stop reparsing selector fragments in match hot paths.
- [ ] P4-003 Add selector prefiltering by rightmost component.
- [ ] P4-004 Cache `@media` and `@supports` results outside per-element loops.
- [ ] P4-005 Integrate `@container` into the main cascade flow.
- [ ] P4-006 Improve `@scope` performance and correctness.
- [ ] P4-007 Expand nested at-rule correctness.
- [ ] P4-008 Add attribute-selector flag support and compatibility edges.
- [ ] P4-009 Introduce subtree restyle invalidation.
- [ ] P4-010 Introduce style sharing and reuse strategy.

## Phase 5 — Layout Engine Incrementality

- [ ] P5-001 Reduce repeated intrinsic-size subtree walks.
- [ ] P5-002 Add memoization for min/max content sizing.
- [ ] P5-003 Remove predictable double-layout shrink-wrap paths.
- [ ] P5-004 Correct intrinsic participation filtering for out-of-flow nodes.
- [ ] P5-005 Preserve source text and whitespace canonical forms.
- [ ] P5-006 Stop destructive anonymous-box rewrites in source-like structures.
- [ ] P5-007 Add dirtiness propagation and relayout boundaries.
- [ ] P5-008 Add layout trace spans by mode and subtree.
- [ ] P5-009 Expand flex/grid/table regression suites.
- [ ] P5-010 Prepare fragment/result caching foundations.

## Phase 6 — Paint, Text, Image, and Raster Efficiency

- [ ] P6-001 Reduce full-surface paint assumptions.
- [ ] P6-002 Add retained display-list directionality.
- [ ] P6-003 Add dirty-rect-aware invalidation plumbing.
- [ ] P6-004 Add text width and glyph cache layers.
- [ ] P6-005 Improve text raster quality under DPR scaling.
- [ ] P6-006 Improve image cache complexity and eviction behavior.
- [ ] P6-007 Reduce repeated image tiling and expensive paint loops.
- [ ] P6-008 Add paint-stage subphase traces.
- [ ] P6-009 Bound software filter and blur cost.
- [ ] P6-010 Prepare tiled/raster backing-store evolution.

## Phase 7 — Shell, View, and Presentation Correctness

- [ ] P7-001 Remove main-thread full rerender triggers where avoidable.
- [ ] P7-002 Fix nested scroll presentation correctness.
- [ ] P7-003 Fix horizontal scroll rendering correctness.
- [ ] P7-004 Improve fixed/sticky rendering to avoid snapshot duplication artifacts.
- [ ] P7-005 Improve input overlay positioning under scroll/resize/zoom.
- [ ] P7-006 Improve screenshot/export fidelity relative to renderer truth.
- [ ] P7-007 Expand HiDPI correctness coverage.
- [ ] P7-008 Add shell-level scroll and selection regressions.
- [ ] P7-009 Prepare shell/compositor separation boundaries.
- [ ] P7-010 Reduce shell work done after render completion.

## Phase 8 — Compositor and Layer System

- [ ] P8-001 Introduce layer candidates and retained layer metadata.
- [ ] P8-002 Separate presentation from one giant software bitmap.
- [ ] P8-003 Add retained surfaces for fixed/sticky/animated content.
- [ ] P8-004 Add async scrolling path foundations.
- [ ] P8-005 Add layer-aware hit testing.
- [ ] P8-006 Add partial invalidation to compositing boundaries.
- [ ] P8-007 Introduce compositor-friendly animation paths.
- [ ] P8-008 Reduce shell reliance on copied overlay pixels.
- [ ] P8-009 Add visual regression coverage for layerized paths.
- [ ] P8-010 Build traceability for compositor scheduling.

## Phase 9 — Web Platform Async Semantics

- [ ] P9-001 Correct timer, microtask, event, and observer ordering.
- [ ] P9-002 Correct worker message delivery lifecycle.
- [ ] P9-003 Improve `fetch` streaming and abort semantics.
- [ ] P9-004 Improve `ReadableStream` body behavior toward incremental delivery.
- [ ] P9-005 Correct Mutation/Resize/Intersection observer semantics.
- [ ] P9-006 Remove event-construction hot-path compilation overhead.
- [ ] P9-007 Add API-specific regression tests for ordering contracts.
- [ ] P9-008 Improve host/runtime memory lifecycle for detached structures.
- [ ] P9-009 Harden long-lived JS API cleanup.
- [ ] P9-010 Expand real-site async behavior coverage.

## Phase 10 — Real-Site Feature Parity

- [ ] P10-001 Expand forms and validation support.
- [ ] P10-002 Expand focus, selection, and text input correctness.
- [ ] P10-003 Expand contenteditable/editor compatibility.
- [ ] P10-004 Expand canvas/SVG rendering fidelity.
- [ ] P10-005 Expand media and graphics infrastructure.
- [ ] P10-006 Expand observer-root and viewport API correctness.
- [ ] P10-007 Expand custom elements and Shadow DOM depth.
- [ ] P10-008 Expand CSS modern layout and visual feature completeness.
- [ ] P10-009 Expand navigation/history behavior for SPA-like sites.
- [ ] P10-010 Build targeted top-site regression suites.

## Phase 11 — Security and Policy Surface

- [ ] P11-001 Harden same-origin, CORS, and redirect policy enforcement.
- [ ] P11-002 Expand cookie policy correctness.
- [ ] P11-003 Expand CSP and mixed-content enforcement planning.
- [ ] P11-004 Build security-policy regression suites.
- [ ] P11-005 Add explicit cancellation and permission boundaries.
- [ ] P11-006 Prepare process/isolation boundaries even before multi-process.
- [ ] P11-007 Improve request and response policy telemetry.
- [ ] P11-008 Expand sandboxed and embedded content planning.
- [ ] P11-009 Add failure traces for policy-denied paths.
- [ ] P11-010 Keep policy work measurable rather than aspirational.

## Phase 12 — Storage, Offline, and Persistence

- [ ] P12-001 Raise local/session storage reliability.
- [ ] P12-002 Expand service-worker and offline feature planning.
- [ ] P12-003 Expand cache storage and persistence groundwork.
- [ ] P12-004 Improve storage cleanup and quota planning.
- [ ] P12-005 Add persistence regression and recovery paths.
- [ ] P12-006 Add profile isolation boundaries.
- [ ] P12-007 Expand real-site offline semantics coverage.
- [ ] P12-008 Instrument storage latency and corruption signals.
- [ ] P12-009 Improve blob URL lifecycle control.
- [ ] P12-010 Keep storage features connected to policy constraints.

## Phase 13 — Accessibility and International Fidelity

- [ ] P13-001 Expand accessibility tree planning.
- [ ] P13-002 Improve selection/caret semantics for assistive tooling.
- [ ] P13-003 Improve bidi/RTL/vertical writing correctness.
- [ ] P13-004 Improve complex-text shaping regression coverage.
- [ ] P13-005 Improve locale-sensitive input and formatting behavior.
- [ ] P13-006 Add a11y-oriented smoke scenarios.
- [ ] P13-007 Prepare native accessibility bridge boundaries.
- [ ] P13-008 Improve keyboard navigation fidelity.
- [ ] P13-009 Expand text metrics tests for non-Latin scripts.
- [ ] P13-010 Make internationalization measurable in CI.

## Phase 14 — GPU and Advanced Rendering Infrastructure

- [ ] P14-001 Plan GPU upload/compositing migration boundaries.
- [ ] P14-002 Separate software fallback from future GPU paths.
- [ ] P14-003 Add renderer/compositor abstractions that do not assume CPU-only raster.
- [ ] P14-004 Build cost accounting for software-only effects.
- [ ] P14-005 Prepare texture/surface lifecycle planning.
- [ ] P14-006 Add trace points for future GPU adoption bottlenecks.
- [ ] P14-007 Expand media/rendering coordination planning.
- [ ] P14-008 Keep software correctness stable while abstractions change.
- [ ] P14-009 Prevent premature GPU complexity before shell/compositor boundaries are solid.
- [ ] P14-010 Ensure every abstraction change pays for itself in measurable simplicity.

## Phase 15 — Developer Ergonomics and Diagnostics

- [ ] P15-001 Expand trace coverage across all pipeline stages.
- [ ] P15-002 Add workload-to-phase mapping in ledger summaries.
- [ ] P15-003 Add better smoke-site harnesses and fixtures.
- [ ] P15-004 Add visual diff infrastructure where missing.
- [ ] P15-005 Add CI failure triage helpers and machine-readable outputs.
- [ ] P15-006 Improve branch/PR/CI loop stability.
- [ ] P15-007 Improve launchd/tmux/watch tooling resilience.
- [ ] P15-008 Add audit loops for structural regressions.
- [ ] P15-009 Keep estate handoff concise but complete.
- [ ] P15-010 Prevent orchestration drift from the actual engine roadmap.

## Phase 16 — Broad Modern Engine Convergence

- [ ] P16-001 Keep all prior phases integrated, not merely locally green.
- [ ] P16-002 Measure broad real-site rendering success repeatedly.
- [ ] P16-003 Measure performance under representative long-scroll, large-DOM, and SPA scenarios.
- [ ] P16-004 Measure memory retention under long sessions and repeated navigation.
- [ ] P16-005 Measure crash-free runtime under prolonged orchestration.
- [ ] P16-006 Keep planner choices aligned with remaining capability gaps rather than novelty.
- [ ] P16-007 Keep shell, renderer, network, and runtime layers independently evolvable.
- [ ] P16-008 Keep tests, traces, and site corpus in step with capability growth.
- [ ] P16-009 Keep CI and local estate execution equivalent enough to trust auto-fix loops.
- [ ] P16-010 Continue converting broad roadmap items into atomic cycle-ready workloads until the remaining gap is genuinely narrow.

## Planner Tie-Break Rules

- Prefer the earliest unfinished phase that currently blocks multiple later phases.
- Prefer one enabling infrastructure fix over one isolated feature unless the isolated feature is a known top-site blocker.
- Prefer workloads that land code and tests together.
- Prefer workloads that reduce full-document rerender, main-thread blockage, or incorrect async semantics.
- Prefer phase-aligned debt reduction over speculative large rewrites.

## Exit Discipline

- Never claim a phase complete without tests and real validation.
- Never pick six workloads that all touch the same shared file cluster.
- Never let a cycle drift away from this document for more than one opportunistic slot.
- Always translate phase progress back into the canonical estate ledger.
