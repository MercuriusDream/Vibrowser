# Symphony Service (Spec-Oriented Implementation)

This directory contains a Python implementation of the Symphony service defined in:
`https://github.com/openai/symphony/blob/main/SPEC.md`.

## What is implemented

- `WORKFLOW.md` loader with YAML front matter parsing and strict template rendering (`issue`, `attempt`)
- Typed config layer with defaults and `$VAR` resolution
- Dynamic workflow reload (`reload_if_changed`) with last-known-good fallback
- Linear tracker adapter:
  - candidate fetch
  - fetch by states
  - state refresh by IDs
- Workspace manager:
  - per-issue deterministic workspaces
  - identifier sanitization
  - containment + cwd safety invariants
  - hooks (`after_create`, `before_run`, `after_run`, `before_remove`) with timeout
- Codex app-server client:
  - `initialize` -> `initialized` -> `thread/start` -> `turn/start`
  - line-delimited JSON stdout protocol
  - request/turn timeouts
  - auto-approval behavior
  - unsupported tool call rejection
  - user-input-required hard-fail
- Orchestrator:
  - polling loop
  - dispatch eligibility/sort
  - global + per-state concurrency
  - reconciliation (stall + tracker state refresh)
  - continuation retry (1s) and exponential backoff retry
  - startup terminal workspace cleanup
  - structured logging + runtime snapshot
- CLI:
  - optional positional workflow path
  - defaults to `./WORKFLOW.md`

## Trust/Safety posture

Current behavior is **high-trust**:

- approval requests are auto-approved for the session
- unsupported tool calls are rejected and session continues
- user-input-required signals fail the run immediately

Run this service only in environments where that posture is acceptable.

## Quick start (scripts)

```bash
cd symphony
./scripts/setup.sh
./scripts/run.sh
```

Single tick for smoke checks:

```bash
./scripts/run-once.sh
```

`setup.sh` will:
- create `WORKFLOW.md` from `WORKFLOW.example.md` (if missing)
- prompt for `LINEAR_API_KEY` and `LINEAR_PROJECT_SLUG`
- store them in `.env.symphony` (mode `600`)

## Manual run

```bash
cd symphony
python3 -m pip install -e .
symphony ./WORKFLOW.md
```
