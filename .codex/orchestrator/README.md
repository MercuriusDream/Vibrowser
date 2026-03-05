# Codex Estate Orchestrator

This directory contains the external supervisor used to run Vibrowser as an ultra-long-horizon Codex CLI workload.

## Model policy

- Main orchestrator: `gpt-5.4` / `high` / fast enabled
- Worker model: `gpt-5.4` / `medium` or `high`
- Worker fast mode: enabled
- External worker fan-out: `6`

## Runtime shape

Each cycle is orchestrated as:

1. Planner chooses 6 atomic workloads from `.codex/codex-estate.md`.
2. Planner grounds those workloads in `.codex/orchestrator/ultra-long-horizon-workload-map.md`.
3. Planner also anchors long-range sequencing against `.codex/orchestrator/phase16-master-checklist.md`.
4. Six external Codex worker sessions implement those workloads in parallel.
5. Integrator reconciles the combined diff.
6. Verifier runs `tools/codex/pr-ready.sh`.
7. Fixer gets one recovery pass if verification fails.
8. Optional branch/PR/CI polling runs after local verification.
9. The canonical Codex ledger is updated and synced to `.claude/claude-estate.md`.

## Commands

```bash
bash .codex/orchestrator/migrate-ledger.sh
bash .codex/orchestrator/supervisor.sh
bash .codex/orchestrator/start-full-autonomy-fast.sh
bash .codex/orchestrator/tmux-launch.sh
bash .codex/orchestrator/tmux-stop.sh
bash .codex/orchestrator/install-launchd.sh
```
