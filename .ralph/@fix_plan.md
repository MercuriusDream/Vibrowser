# Ralph Fix Plan

## Stories to Implement

### [Epic 1: Session Control and Deterministic Lifecycle]
> Goal: Reliable session control, deterministic navigation lifecycle, and reproducible load/recovery behavior.

- [x] Story 1.1: Start a browser session from URL, file path, and local resource URI
  > As a developer or maintainer, start sessions from URL, local path, and `file://` inputs through one deterministic flow.
  > AC: Given the CLI or shell session is idle and input includes a URL, path, or `file://` URI, When I launch a session with that input, Then the engine enters an active session with normalized input metadata.

- [x] Story 1.2: Expose lifecycle state transitions in diagnostics
  > As a developer, observe every major lifecycle stage (fetch/parse/style/layout/render) as visible structured diagnostics.
  > AC: Given a session is active, When the engine advances through fetch, parse, style, layout, and render, Then each stage emits a visible stage event with status, timestamp, and severity.

- [x] Story 1.3: Retry failed navigation and preserve session context
  > As a user, recover failed sessions without losing session state.
  > AC: Given a session is in a failed or error state, When I trigger Retry, Then the engine restarts at the failed stage or earlier and retains prior context.

- [x] Story 1.4: Cancel running navigation safely
  > As a developer, stop in-flight navigation or processing quickly and safely.
  > AC: Given navigation or stage processing is in-flight, When I invoke Cancel, Then the engine transitions to a safe stopped state without partial locks and logs terminal-safe event.

- [x] Story 1.5: Reproduce deterministic lifecycle transitions for the same input
  > As an engineer, rerun fixtures and compare deterministic lifecycle behavior.
  > AC: Given a previously captured normalized input and configuration are available, When I run the same input again, Then lifecycle transitions and stage-level timing are reproducible within expected variance.

### [Epic 2: Parsing and Styling]
> Goal: Deterministic parsing and styling with recoverable malformed-input behavior.

- [x] Story 2.1: Parse supported HTML into deterministic DOM structures
  > As a contributor, produce stable DOM outputs for supported input.
  > AC: Given an input HTML fixture within supported scope, When parsing is executed, Then the resulting document model is deterministic.

- [x] Story 2.2: Recover deterministically from malformed HTML
  > As a contributor, keep parsing deterministic when malformed HTML is encountered.
  > AC: Given malformed but parseable HTML is provided, When parsing executes, Then parser emits documented recovery path and warning and continues without silent corruption.

- [x] Story 2.3: Apply selectors and style rules with deterministic resolution
  > As an engineer, produce stable style output from supported selectors.
  > AC: Given DOM and stylesheet inputs are in supported form, When style computation runs, Then selector matching and cascade behavior are deterministic and stable.

- [x] Story 2.4: Handle selector/media fallback behavior explicitly
  > As a developer, remain deterministic when unsupported selectors or media conditions are encountered.
  > AC: Given unsupported or partially supported selectors/media conditions are encountered, When style resolution runs, Then fallback rules are deterministic and logged.

- [x] Story 2.5: Verify DOM and style transition consistency
  > As a contributor, validate stage consistency from parse to style.
  > AC: Given a parsed and styled fixture runs through lifecycle, When transitions occur between parse and style stages, Then snapshots show valid node and property transitions.

### [Epic 3: Layout and Rendering]
> Goal: Stable, reproducible layout and rendering with exportable artifacts and transition tracing.

- [x] Story 3.1: Compute stable block, inline, and positioned layout within scoped grammar
  > As an engineer, produce consistent layout geometry for supported constructs.
  > AC: Given styled DOM trees with supported flow/positioned constructs, When layout runs, Then block/inline/positioned boxes are produced consistently.

- [x] Story 3.2: Keep geometry stable for repeated fixture runs
  > As a maintainer, keep deterministic geometry under repeated execution.
  > AC: Given fixture input has not changed, When the same run is repeated, Then geometry remains within deterministic tolerance.

- [x] Story 3.3: Render visual output through both headless and shell presentation paths
  > As a developer, render output in selected sink modes.
  > AC: Given rendering finishes successfully, When presentation mode is headless or shell, Then output is produced through configured sink.

- [x] Story 3.4: Export render artifacts and render metadata
  > As a contributor, capture render outputs and metadata for inspection.
  > AC: Given a render completes successfully, When export is requested, Then artifact and basic metadata are written to configured path.

- [x] Story 3.5: Trace layout and paint transitions per fixture
  > As an engineer, correlate layout and paint transitions for performance and correctness.
  > AC: Given a fixture enters render stage, When trace mode is enabled, Then system records layout-to-paint transitions with timings and stage labels.

### [Epic 4: Interaction and Runtime Mutation]
> Goal: Controlled runtime interaction and mutation with deterministic re-render behavior.

- [x] Story 4.1: Implement controlled JavaScript bridge element queries
  > As a runtime user, run element queries through deterministic bridge behavior.
  > AC: Given an active DOM is available, When a bridge query API is invoked for supported selectors, Then deterministic element references and values are returned.

- [x] Story 4.2: Mutate style and attributes via runtime bridge calls
  > As a developer, apply safe runtime mutations through the bridge.
  > AC: Given a runtime mutation request is issued, When style or attributes are applied, Then updates are processed deterministically through update pipeline.

- [x] Story 4.3: Handle supported input and event updates
  > As a user, handle supported input and event updates deterministically.
  > AC: Given input actions map to supported events, When events are dispatched, Then handlers mutate state through deterministic event flow.

- [x] Story 4.4: Re-render output after runtime DOM/style mutations
  > As a developer, ensure mutations trigger appropriate downstream updates.
  > AC: Given a layout-affecting mutation changes data, When mutation commits, Then layout and render re-run for affected subtree or region.

- [x] Story 4.5: Provide deterministic mutation regression fixtures
  > As a maintainer, guard runtime mutation behavior with fixtures.
  > AC: Given scripted interaction fixtures exist, When the regression suite runs, Then expected mutations and re-render outputs match snapshot and timing tolerances.

### [Epic 5: Networking, Caching, and Policy Enforcement]
> Goal: Deterministic request lifecycle, caching behavior, and policy enforcement.

- [x] Story 5.1: Implement request/response lifecycle contracts
  > As a contributor, enforce explicit request/response metadata for all network transactions.
  > AC: Given a remote resource request is initiated, When dispatch/receive/complete occurs, Then request and response metadata follows a defined contract.

- [x] Story 5.2: Add deterministic caching policy behavior
  > As an engineer, ensure repeat requests follow configured cache policy.
  > AC: Given cache policy and URL set are configured, When same resource is requested repeatedly, Then cached responses are reused per policy.

- [x] Story 5.3: Load linked CSS resources with deterministic fallback
  > As a developer, support linked CSS loading with explicit integration semantics.
  > AC: Given HTML references linked CSS resources, When they load, Then deterministic fetch, parse, and style integration occurs.

- [x] Story 5.4: Enforce redirects, origin boundaries, and request constraints
  > As a maintainer, protect request behavior with policy checks.
  > AC: Given redirects/cross-origin conditions exist, When request handling enforces policy, Then violating requests are rejected or flagged deterministically.

- [x] Story 5.5: Provide request/response boundary fixtures for tests
  > As a developer, validate boundary rules through tests.
  > AC: Given fixtures define allowed/disallowed boundary cases, When fixture suite runs, Then each policy case passes only when implementation matches boundary rules.

### [Epic 6: Observability, Testing, and Quality Gates]
> Goal: Observability, reproducibility, quality evidence, privacy controls, and reliable troubleshooting.

- [x] Story 6.1: Emit structured diagnostics with severity, module, and stage context
  > As an operator, receive observability with actionable severity and provenance.
  > AC: Given an error or warning occurs, When diagnostics are emitted, Then each event includes timestamp, severity, module, stage, and correlation metadata.

- [x] Story 6.2: Track milestone acceptance evidence from fixture runs
  > As a maintainer, collect milestone evidence from fixture and test runs.
  > AC: Given a milestone checklist is configured, When fixture and test runs complete, Then results produce pass/fail evidence per gate.

- [x] Story 6.3: Make privacy-sensitive behaviors explicit and opt-in only
  > As an operator, retain explicit privacy/telemetry controls.
  > AC: Given default settings, When telemetry/reporting options are not explicitly enabled, Then no optional telemetry data is collected or sent.

- [x] Story 6.4: Collect reproducible failure traces and correlation identifiers
  > As a debugger, capture reproducible traces for failures.
  > AC: Given a failure occurs, When diagnostics and snapshots are recorded, Then trace data is reproducible for same input and configuration.

- [x] Story 6.5: Validate module contracts independently with maintainability tooling
  > As a contributor, verify module boundaries independently.
  > AC: Given module boundaries and interfaces are defined, When contract tests execute, Then each module passes API, boundary, and lifecycle tests independently.

- [x] Story 6.6: Include recovery and replay support for operational troubleshooting
  > As an operator, follow an actionable recovery path for failures.
  > AC: Given a failure is detected with classified stage context, When troubleshooting mode is opened, Then operator gets replay/retry/cancel action path.

## Completed

- [x] Story 1.1: BrowserEngine facade with session lifecycle, input normalization (URL/path/file://), and diagnostic events. New modules: `core/lifecycle`, `core/diagnostics`, `engine/navigation`, `engine/engine`. main.cpp updated to use engine. Smoke tests pass.

## Notes
- Follow TDD methodology (red-green-refactor)
- One story per Ralph loop iteration
- Update this file after completing each story
