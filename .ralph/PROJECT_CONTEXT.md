# codex_browser — Project Context

## Project Goals

- Build a stable, inspectable browser-engine core first (`fetch -> parse -> CSS -> layout -> render`).
- Keep scope narrow in MVP with strict sequencing through milestones.
- Make lifecycle transitions and diagnostics explicit for every major stage.
- Preserve deterministic behavior with recoverability and traceability.
- Keep security/privacy defaults explicit, with controls for opt-in telemetry and reporting.

## Success Metrics

- Onboarding: clone/build and complete one full sample run within 60 minutes.
- Baseline fixture reliability: 95% of curated MVP fixtures complete to rendered output.
- P95 baseline render time for reference fixtures under 2.5 seconds.
- At least 90%+ key lifecycle transitions emit structured diagnostics by default.
- Regression control: avoid hard fixture-pass regressions and keep geometry stable for unchanged fixtures.

## Architecture Constraints

- Platform stack: C++17 + CMake is required canonical foundation.
- Module-first structure is required: `browser`, `net`, `html`, `css`, `layout`, `render`, `js`, `core`.
- Maintain deterministic lifecycle order (`fetch -> parse -> style -> layout -> render`) and explicit stage state contracts.
- Enforce request/response lifecycle contracts with policy-aware networking and explicit request metadata.
- Preserve explicit diagnostics/event contracts for engine and UI/CLI surfaces.
- OpenSSL support is optional and CMake-detected.

## Technical Risks

- Standards breadth can outpace test confidence without milestone gates and feature flags.
- Lifecycle and diagnostics regressions can hide failures without strict stage event discipline.
- Privacy and policy behavior can drift without explicit consent-first defaults.
- Reproducibility loss across parser/layout/render changes risks long-term maintainability.

## Scope Boundaries

- MVP focus: deterministic navigation lifecycle, parsing/style/layout/render pipeline, controlled interaction, network/resource handling, diagnostics, and reproducibility.
- Post-MVP: broader compatibility and richer features only through controlled growth lanes with acceptance gates.
- Out of current scope: full browser platform parity, extensive API surface, advanced isolation primitives, or heavy multi-process architecture.

## Target Users

- Learning-oriented contributors and contributors-to-be building a constrained browser-engine codebase.
- Contributor teams needing reproducible debugging, fixtures, and milestone-based delivery.
- Security/performance reviewers validating policy and network/privacy boundaries.
- Educators and mentors needing explainable implementation progression.

## Non-Functional Requirements

- Milestone quality gates for baseline fixture pass rate and controlled regression windows.
- Deterministic lifecycle and deterministic replay on repeat inputs.
- High-quality diagnostics (severity/module/stage correlation) for all major state transitions.
- Optional telemetry/reporting only when explicitly enabled; safe-by-default privacy behavior.
- Stable interfaces and contract-first module boundaries to reduce architectural churn.
