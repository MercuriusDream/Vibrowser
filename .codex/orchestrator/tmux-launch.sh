#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=.codex/orchestrator/lib.sh
source "$SCRIPT_DIR/lib.sh"

ATTACH=1
MODE="single-session"
SUPERVISOR_ARGS="${CODEX_ESTATE_SUPERVISOR_ARGS:-}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-attach)
      ATTACH=0
      ;;
    --multi-session)
      MODE="multi-session"
      ;;
    --single-session)
      MODE="single-session"
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1
      ;;
  esac
  shift
done

if ! command -v tmux >/dev/null 2>&1; then
  echo "tmux is not installed." >&2
  exit 1
fi

ensure_layout
seed_local_protocols
bootstrap_ledgers
init_state_if_missing

if [[ "$MODE" == "single-session" ]]; then
  if tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
    echo "tmux session already exists: $TMUX_SESSION"
  else
    tmux new-session -d -s "$TMUX_SESSION" -c "$PROJECT_DIR" "/bin/zsh -lc 'bash .codex/orchestrator/supervisor.sh $SUPERVISOR_ARGS'"
    for slot in $(seq 1 "$WORKER_COUNT"); do
      tmux new-window -t "$TMUX_SESSION" -n "worker-$slot" -c "$PROJECT_DIR" "/bin/zsh -lc 'bash .codex/orchestrator/watch-role.sh worker-$slot'"
    done
    tmux new-window -t "$TMUX_SESSION" -n integrator -c "$PROJECT_DIR" "/bin/zsh -lc 'bash .codex/orchestrator/watch-role.sh integrator'"
    tmux new-window -t "$TMUX_SESSION" -n verifier -c "$PROJECT_DIR" "/bin/zsh -lc 'bash .codex/orchestrator/watch-role.sh verifier'"
    tmux new-window -t "$TMUX_SESSION" -n fixer -c "$PROJECT_DIR" "/bin/zsh -lc 'bash .codex/orchestrator/watch-role.sh fixer'"
    tmux new-window -t "$TMUX_SESSION" -n ci-fixer -c "$PROJECT_DIR" "/bin/zsh -lc 'bash .codex/orchestrator/watch-role.sh ci-fixer'"
    tmux new-window -t "$TMUX_SESSION" -n state -c "$PROJECT_DIR" "/bin/zsh -lc 'bash .codex/orchestrator/watch-state.sh'"
  fi
  if [[ $ATTACH -eq 1 ]]; then
    exec tmux attach -t "$TMUX_SESSION"
  fi
  exit 0
fi

base="${TMUX_SESSION}"
tmux new-session -d -s "${base}-supervisor" -c "$PROJECT_DIR" "/bin/zsh -lc 'bash .codex/orchestrator/supervisor.sh $SUPERVISOR_ARGS'"
for slot in $(seq 1 "$WORKER_COUNT"); do
  tmux new-session -d -s "${base}-worker-$slot" -c "$PROJECT_DIR" "/bin/zsh -lc 'bash .codex/orchestrator/watch-role.sh worker-$slot'"
done
tmux new-session -d -s "${base}-integrator" -c "$PROJECT_DIR" "/bin/zsh -lc 'bash .codex/orchestrator/watch-role.sh integrator'"
tmux new-session -d -s "${base}-verifier" -c "$PROJECT_DIR" "/bin/zsh -lc 'bash .codex/orchestrator/watch-role.sh verifier'"
tmux new-session -d -s "${base}-ci-fixer" -c "$PROJECT_DIR" "/bin/zsh -lc 'bash .codex/orchestrator/watch-role.sh ci-fixer'"
tmux new-session -d -s "${base}-state" -c "$PROJECT_DIR" "/bin/zsh -lc 'bash .codex/orchestrator/watch-state.sh'"

if [[ $ATTACH -eq 1 ]]; then
  exec tmux attach -t "${base}-supervisor"
fi
