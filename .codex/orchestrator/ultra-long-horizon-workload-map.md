# Vibrowser Ultra-Long-Horizon Workload Map

This file is the orchestration-ready backlog map for the Codex CLI supervisor.

## Planner Contract

- The planner must read this file every cycle.
- Prefer 6 workloads that are atomic, disjoint, and verifiable in one cycle.
- Favor unfinished work already referenced by the estate ledger.
- Fill empty worker slots with debt-paydown, test expansion, or observability work.
- If a workload needs more than one cycle, split it before assignment.

## Workload Shape

Each chosen workload should be recorded in planner output with:

- Title
- Objective
- Owned paths
- Verification
- Prompt seed

## Priority Bands

### Band A — Engine Correctness and Runtime Safety

- A1. JS thread affinity
- A2. event loop correctness
- A3. timer/microtask ordering
- A4. DOM mutation ordering
- A5. render pipeline determinism
- A6. network protocol correctness

### Band B — Modern Rendering and Performance

- B1. style invalidation
- B2. incremental layout
- B3. retained paint
- B4. compositor layers
- B5. async scrolling
- B6. image/font/text caches

### Band C — Real-World Platform Support

- C1. forms and input
- C2. observers and async APIs
- C3. media and graphics
- C4. storage and offline
- C5. web components
- C6. accessibility

### Band D — Orchestration and Reliability

- D1. build/test speed
- D2. CI robustness
- D3. trace coverage
- D4. smoke-site corpus
- D5. estate ledger hygiene
- D6. launch/recovery automation

## Ownership-Oriented Workload Pools

### Pool 1 — Runtime / JS Host

Owned paths examples:

- `vibrowser/src/js/**`
- `vibrowser/src/platform/**`
- `vibrowser/src/ipc/**`
- `vibrowser/include/clever/platform/**`
- `vibrowser/include/clever/ipc/**`

Atomic workload templates:

- Tighten WebSocket delivery onto main JS runtime only.
- Replace host-driven timer flush semantics with scheduled task queue behavior.
- Remove synchronous observer delivery from DOM mutation hot paths.
- Add regression tests for microtask, timer, and observer ordering.
- Stabilize worker message pump and cleanup lifecycle.
- Add runtime traces for long task detection and event-loop lag.

### Pool 2 — Network / Fetch / Cache

Owned paths examples:

- `vibrowser/src/net/**`
- `vibrowser/include/clever/net/**`
- `vibrowser/src/url/**`
- `vibrowser/src/js/js_fetch_bindings.cpp`

Atomic workload templates:

- Fix HTTP/2 protocol deadlock or flow-control edge case.
- Improve redirect URL resolution through base URL merge.
- Normalize cache keys and eliminate fragment-sensitive duplication.
- Add DNS/connect latency tests or connection reuse regression tests.
- Convert a synchronous fetch edge to incremental/streaming behavior.
- Harden cookie host-only/path semantics with focused unit coverage.

### Pool 3 — Style / Selector Engine

Owned paths examples:

- `vibrowser/src/css/style/**`
- `vibrowser/src/css/parser/**`
- `vibrowser/tests/unit/css_*`

Atomic workload templates:

- Cache parsed inline styles.
- Precompute selector specificity on parse.
- Stop reparsing dynamic pseudo selectors in hot match paths.
- Add rule-bucket indexing for one selector family.
- Wire one restyle invalidation path for dynamic pseudo-class changes.
- Expand nested at-rule parsing correctness coverage.

### Pool 4 — Layout Engine

Owned paths examples:

- `vibrowser/src/layout/**`
- `vibrowser/tests/unit/layout_*`

Atomic workload templates:

- Memoize one intrinsic size path.
- Remove one destructive layout mutation on source node data.
- Fix one out-of-flow participation bug in intrinsic measurement.
- Add incremental relayout dirtiness tests.
- Reduce one double-layout shrink-wrap path.
- Expand table/grid/flex regression coverage.

### Pool 5 — Paint / Text / Renderer

Owned paths examples:

- `vibrowser/src/paint/**`
- `vibrowser/tests/unit/paint_*`

Atomic workload templates:

- Eliminate one full-surface paint dependency.
- Add cache for one expensive text measurement path.
- Improve DPR-aware text/image rendering fidelity.
- Reduce one filter/backdrop CPU hotspot.
- Convert one repeated image draw path to cache-aware behavior.
- Add trace spans for paint stage subphases.

### Pool 6 — Shell / Compositor / UX

Owned paths examples:

- `vibrowser/src/shell/**`
- `build_launch_app.sh`
- `build_test_bin.sh`

Atomic workload templates:

- Remove one main-thread full rerender trigger.
- Fix one nested scroll or fixed/sticky shell correctness gap.
- Align screenshot/export with renderer truth rather than cached viewport.
- Improve input overlay positioning under scroll/resize/zoom.
- Introduce one layerization or retained-surface primitive.
- Add shell-level regression coverage for HiDPI or scrolling behavior.

## Phase-Oriented Master Roadmap

### Phase 1 — Stabilize the Runtime Spine

- P1-001. Enforce JS thread affinity across WebSocket, worker, and fetch callbacks.
- P1-002. Make event-loop deadlines wake correctly when earlier delayed tasks arrive.
- P1-003. Split task sources by timer, input, render, network, and idle.
- P1-004. Make timer cancellation cheap and repeating timers drift-resistant.
- P1-005. Replace blocking IPC semantics with bounded async-style interfaces.
- P1-006. Add event-loop starvation and lag telemetry.

### Phase 2 — Make Networking Browser-Shaped

- P2-001. Remove request-path deadlocks and broken HTTP/2 frame semantics.
- P2-002. Add request prioritization by resource class.
- P2-003. Add DNS caching and connection establishment improvements.
- P2-004. Move toward progressive response parsing.
- P2-005. Make fetch more abortable and streaming-friendly.
- P2-006. Raise cache and cookie behavior toward browser-grade correctness.

### Phase 3 — Rebuild Style and Layout Around Invalidation

- P3-001. Compile selectors once and reuse them.
- P3-002. Add rightmost selector prefiltering.
- P3-003. Cache global conditionals such as `@media` and `@supports`.
- P3-004. Introduce subtree restyle invalidation.
- P3-005. Reduce intrinsic measurement duplication.
- P3-006. Start fragment/result caching in layout.

### Phase 4 — Rebuild Paint and Compositing

- P4-001. Introduce dirty rect propagation.
- P4-002. Convert full-surface rendering into retained/tiled rendering.
- P4-003. Add text/image/glyph caches.
- P4-004. Replace snapshot-based fixed/sticky compositing.
- P4-005. Introduce compositor-aware animation paths.
- P4-006. Separate shell presentation from renderer truth.

### Phase 5 — Complete Modern Web Platform Gaps

- P5-001. Finish observers and async DOM API correctness.
- P5-002. Raise forms/input/editor behavior.
- P5-003. Expand storage/offline features.
- P5-004. Expand media/graphics support.
- P5-005. Expand shadow DOM/custom elements correctness.
- P5-006. Expand accessibility and international text fidelity.

### Phase 6 — Production Orchestration and Continuous Delivery

- P6-001. Make build/test/bench/smoke deterministic in CI.
- P6-002. Auto-branch and auto-PR successful cycles.
- P6-003. Poll CI and re-enter fixer cycles for actionable failures.
- P6-004. Keep estate ledger and workload map synchronized.
- P6-005. Run launchd/tmux-based perpetual orchestration locally.
- P6-006. Keep structural regression audits recurring.

## Micro-Task Catalog

### Runtime Micro-Tasks

- [ ] Add a regression test for background-thread JS callback rejection.
- [ ] Add a regression test for timer wake-up after earlier delayed-task insertion.
- [ ] Record event-loop lag in one trace output path.
- [ ] Limit one IPC message allocation path with a size cap.
- [ ] Remove one sleep-based polling loop from worker or websocket teardown.
- [ ] Add cleanup coverage for one long-lived JS host map.

### Network Micro-Tasks

- [ ] Add unit coverage for one HTTP/2 SETTINGS ACK case.
- [ ] Add unit coverage for one `WINDOW_UPDATE` accounting case.
- [ ] Normalize one cache-key URL canonicalization path.
- [ ] Refactor one redirect merge path to use URL parser helpers.
- [ ] Add cookie host-only behavior coverage.
- [ ] Add one streaming-oriented fetch regression test scaffold.

### Style Micro-Tasks

- [ ] Cache inline style parse results for one resolve path.
- [ ] Stop reparsing `:is()` arguments in one match path.
- [ ] Stop reparsing `:not()` arguments in one match path.
- [ ] Cache one `@media` decision outside per-element loops.
- [ ] Cache one `@supports` decision outside per-element loops.
- [ ] Add selector bucket prefiltering for class selectors.

### Layout Micro-Tasks

- [ ] Memoize one min-content computation path.
- [ ] Memoize one max-content computation path.
- [ ] Fix one shrink-wrap double layout path.
- [ ] Exclude one non-participating descendant type from intrinsic measurement.
- [ ] Preserve one source text field instead of mutating it in place.
- [ ] Add one subtree dirtiness regression test.

### Paint Micro-Tasks

- [ ] Cache one text width measurement path.
- [ ] Reuse one bitmap/context allocation path.
- [ ] Add one display-list trace counter.
- [ ] Improve one repeated background tiling path.
- [ ] Add one image cache eviction complexity improvement.
- [ ] Reduce one blur/filter region pass cost.

### Shell Micro-Tasks

- [ ] Remove one hover-triggered full rerender.
- [ ] Fix one nested scroll presentation path.
- [ ] Fix one horizontal scroll path.
- [ ] Improve one fixed/sticky duplication artifact.
- [ ] Improve one input overlay positioning edge case.
- [ ] Add one shell HiDPI regression test.

### Orchestration Micro-Tasks

- [ ] Keep `.codex/codex-estate.md` canonical and `.claude/claude-estate.md` shadow-synced.
- [ ] Keep planner outputs schema-valid every cycle.
- [ ] Keep all 6 workloads disjoint by path ownership.
- [ ] Keep `tools/codex/pr-ready.sh` green before branch/PR.
- [ ] Keep CI polling actionable and non-blocking when no checks exist.
- [ ] Keep launchd/tmux bootstrap scripts healthy.

## Planner Heuristics

- Prefer one atomic runtime fix over one broad speculative refactor.
- Prefer code + regression test together.
- Prefer unfinished work already mentioned in the estate ledger.
- Prefer six disjoint workloads over six semantically related but conflicting edits.
- Prefer verification and debt-paydown slots when ownership would otherwise collide.
- Prefer one cycle’s summary to be obvious from workload titles alone.

## Integrator Heuristics

- Shared-file edits belong to integrator, not workers.
- If two workers imply a shared abstraction, integrate it conservatively.
- Never widen a fix unless verification proves it is required.
- Preserve working worker code and trim only conflicts or regressions.

## Verifier Heuristics

- Always run clean build/test/bench/smoke through `tools/codex/pr-ready.sh`.
- Treat missing CI configuration as a fixable orchestration gap, not as silent success.
- Record every failure path into the verifier log so fixer prompts stay concrete.

## Success Criteria

- A planner can pick 6 workloads from this map without inventing structure.
- Each workload can be handed to one worker with disjoint paths.
- The integrator can reconcile the outputs in one pass.
- The verifier can run without human interpretation.
- The estate ledger can summarize the cycle from the chosen workloads.
