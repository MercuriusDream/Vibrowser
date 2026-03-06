# Codex Estate Orchestrator

This directory contains the external supervisor used to run Vibrowser as an ultra-long-horizon Codex CLI workload.

`supervisor-runner.sh` is the durable entrypoint. It restarts `supervisor.sh` after unexpected exits and propagates `Ctrl+C`/termination into the supervisor and its child worker processes.

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
7. Fixer stays in the same cycle and keeps repairing until verification passes.
8. Optional branch/PR/CI polling runs after local verification.
9. The canonical Codex ledger is updated and synced to `.claude/claude-estate.md`.

## Commands

```bash
bash .codex/orchestrator/migrate-ledger.sh
bash .codex/orchestrator/supervisor.sh
bash .codex/orchestrator/start-full-autonomy-fast.sh
zsh .codex/orchestrator/start-full-autonomy-fast.zsh
bash .codex/orchestrator/resume-live.sh
bash .codex/orchestrator/resume-unattended-full-cicd.sh
bash .codex/orchestrator/tmux-launch.sh
bash .codex/orchestrator/tmux-stop.sh
bash .codex/orchestrator/detect-writer.sh
bash .codex/orchestrator/safe-restart.sh
bash .codex/orchestrator/install-launchd.sh
```

## Safe restart

1. `bash .codex/orchestrator/detect-writer.sh`
2. `bash .codex/orchestrator/tmux-stop.sh`
3. `bash .codex/orchestrator/uninstall-launchd.sh`
4. `bash .codex/orchestrator/detect-writer.sh`
5. `zsh .codex/orchestrator/start-full-autonomy-fast.zsh`
6. `bash .codex/orchestrator/install-launchd.sh`

`safe-restart.sh` prints this exact sequence by default. It only performs the stop half when run with `--execute`, and still intentionally does not auto-start a new writer.

## One-command resume + live monitoring

Use:

```bash
bash .codex/orchestrator/resume-live.sh
```

This single entry point:

1. Clears the stop file
2. Kills stale tmux/background estate writers first
3. Unloads the launchd writer by default to avoid double-writing
4. Reuses the existing cycle/state ledger
5. Starts a fresh tmux-backed live estate view and attaches immediately

The `state` tmux window now shows:

- current cycle state JSON
- the active run directory
- parsed summaries for `plan.json`, `integrator.json`, `verifier.json`, `git-pr.json`, `ci.json`, `fixer.json`, and `ci-fixer.json` when present
- short tails for `verifier.log`, `ci.log`, `ci-fixer.log`, and `git-pr.log`

The tmux session also includes a dedicated `ci-fixer` window so CI-repair activity is visible without inferring it from the generic `fixer` pane.

## Unattended full CICD

Use:

```bash
bash .codex/orchestrator/resume-unattended-full-cicd.sh
```

This turns on:

- autogit
- push
- PR creation
- CI wait
- CI autofix
- known-baseline test allowance for the current red-suite list
