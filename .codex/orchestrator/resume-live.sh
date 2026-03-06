#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=.codex/orchestrator/lib.sh
source "$SCRIPT_DIR/lib.sh"

KEEP_LAUNCHD=0
NO_ATTACH=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --keep-launchd)
      KEEP_LAUNCHD=1
      ;;
    --no-attach)
      NO_ATTACH=1
      ;;
    *)
      echo "Unknown argument: $1" >&2
      echo "Usage: bash .codex/orchestrator/resume-live.sh [--keep-launchd] [--no-attach]" >&2
      exit 1
      ;;
  esac
  shift
done

export CODEX_ESTATE_MAIN_MODEL="${CODEX_ESTATE_MAIN_MODEL:-gpt-5.4}"
export CODEX_ESTATE_MAIN_REASONING="${CODEX_ESTATE_MAIN_REASONING:-high}"
export CODEX_ESTATE_MAIN_FAST_FLAG="${CODEX_ESTATE_MAIN_FAST_FLAG:-1}"
export CODEX_ESTATE_SUBAGENT_MODEL="${CODEX_ESTATE_SUBAGENT_MODEL:-gpt-5.4}"
export CODEX_ESTATE_SUBAGENT_REASONING="${CODEX_ESTATE_SUBAGENT_REASONING:-medium}"
export CODEX_ESTATE_SUBAGENT_FAST_FLAG="${CODEX_ESTATE_SUBAGENT_FAST_FLAG:-1}"
export CODEX_ESTATE_WORKERS="${CODEX_ESTATE_WORKERS:-6}"

if [[ -f "$STOP_FILE" ]]; then
  rm -f "$STOP_FILE"
fi

if [[ $KEEP_LAUNCHD -eq 0 ]]; then
  launchctl unload "$HOME/Library/LaunchAgents/com.vibrowser.codex-estate.plist" >/dev/null 2>&1 || true
fi

cat <<EOF
Resuming Codex Estate live session:
  main model      : $CODEX_ESTATE_MAIN_MODEL
  main reasoning  : $CODEX_ESTATE_MAIN_REASONING
  main fast       : $CODEX_ESTATE_MAIN_FAST_FLAG
  worker model    : $CODEX_ESTATE_SUBAGENT_MODEL
  worker reasoning: $CODEX_ESTATE_SUBAGENT_REASONING
  worker fast     : $CODEX_ESTATE_SUBAGENT_FAST_FLAG
  workers         : $CODEX_ESTATE_WORKERS
  current cycle   : $(jq -r '.cycle // 0' "$CURRENT_STATE_FILE" 2>/dev/null || echo 0)
  next cycle      : $(( $(jq -r '.cycle // 0' "$CURRENT_STATE_FILE" 2>/dev/null || echo 0) + 1 ))
  launchd writer  : $([[ $KEEP_LAUNCHD -eq 1 ]] && echo "kept" || echo "unloaded")
EOF

if tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
  echo "Attaching to existing tmux session: $TMUX_SESSION"
  if [[ $NO_ATTACH -eq 1 ]]; then
    exit 0
  fi
  exec tmux attach -t "$TMUX_SESSION"
fi

if [[ $NO_ATTACH -eq 1 ]]; then
  exec bash "$SCRIPT_DIR/tmux-launch.sh" --no-attach
fi

exec bash "$SCRIPT_DIR/tmux-launch.sh"
