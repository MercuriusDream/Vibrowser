# Full Traversal Coverage Ledger

Generated from the already-collected wave evidence at:
- `audit/wave0/file-census.md`
- `audit/wave0/build-graph.md`
- `audit/wave0/third-party-linkage.md`
- `audit/wave0/coverage-ledger.md`
- `audit/wave1/wave1a-from-scratch-architecture.md`
- `audit/wave1/wave1b-clever-architecture.md`

## Wave Status

- Wave 0 (Census/Routing): completed
- Wave 1 (Architecture Reconstruction): completed
- Wave 2 (Standards Compliance Mapping): completed
- Wave 3 (Rendering + JS Deep Dive): completed
- Wave 4 (Networking/Security/Robustness): completed
- Wave 5 (Performance/Refactorability): not yet executed in files
- Wave 6 (Synthesis/Adversarial): completed

## Scope Coverage Snapshot

| Subsystem | Scope | Evidence Source | Status |
|---|---|---|---|
| from_scratch core engine | `src/`, `include/` | `audit/wave0/file-census.md:1`, `audit/wave1/wave1a-from-scratch-architecture.md:1` | Covered (partial) |
| clever engine | `clever/src/`, `clever/include/` | `audit/wave0/file-census.md:1`, `audit/wave1/wave1b-clever-architecture.md:1` | Covered (partial) |
| tests | `tests/`, `clever/tests/` | `audit/wave0/file-census.md:1`, `audit/wave0/build-graph.md:1` | Covered |
| docs/planning | `docs/`, `clever/docs/`, `_bmad-output/` | `audit/wave0/file-census.md:1`, `audit/wave0/third-party-linkage.md:1`, `README.md:1` | Covered |
| generated/build artifacts | `build/`, `clever/build/` | `audit/wave0/file-census.md:1`, `audit/wave0/coverage-ledger.md:1` | Classified as generated |

## Build/Cross-Repo Graph Coverage

- Root build graph evidence: `audit/wave0/build-graph.md:1`
- Clever build graph evidence: `audit/wave0/build-graph.md:1`
- Test target inclusion evidence: `audit/wave0/build-graph.md:1`
- Third-party integration evidence: `audit/wave0/third-party-linkage.md:1`

## Directory Classification Policy Applied

- `source`: first-party implementation and headers
- `test`: unit/integration tests and test fixtures
- `docs`: README, roadmap, blueprint, and planning artifacts
- `generated`: CMake build output directories and compiled outputs
- `metadata`: planning/state artifacts and audit-ledger files

## Deterministic manifest IDs

Manifest entries are recorded by `<subsystem>/<relative_path>` for traceability.  
Examples:
- `core/src/...`
- `clever/src/...`
- `core/tests/...`
- `docs/...`
- `specs/_bmad-output/...`

## Warnings

- Remaining missing wave is Wave 5 only; Waves 2, 3, 4, and 6 now have first-party review evidence artifacts.
- This ledger now reflects generated audit outputs from Wave 2, Wave 3, Wave 4, and Wave 6 review files.
