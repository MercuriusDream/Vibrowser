# Codex Estate Local Protocol

- Canonical ledger: `.codex/codex-estate.md`
- Shadow ledger: `.claude/claude-estate.md`
- Main orchestrator runtime: `gpt-5.4` / `high`
- Worker runtime: `gpt-5.4` / `medium` or `high`
- Worker fast mode: enabled
- External worker fan-out: `6`
- Cycle contract: planner -> worker-1..worker-6 -> integrator -> verifier -> fixer (max one round) -> ledger sync
- Verification gate: `tools/codex/pr-ready.sh`
- Stop file: `.claude/.claude-estate-stop`
- Human live view: `.codex/orchestrator/tmux-launch.sh`
