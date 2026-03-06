#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=.codex/orchestrator/lib.sh
source "$SCRIPT_DIR/lib.sh"

echo "Codex Estate writer inspection"
echo "project: $PROJECT_DIR"
echo

echo "[launchd]"
if launchctl list | grep -Fq "com.vibrowser.codex-estate"; then
  launchctl list | grep -F "com.vibrowser.codex-estate"
else
  echo "not loaded"
fi
echo

echo "[tmux]"
if command -v tmux >/dev/null 2>&1; then
  if tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
    tmux list-windows -t "$TMUX_SESSION"
  else
    echo "session $TMUX_SESSION not found"
  fi
else
  echo "tmux not installed"
fi
echo

echo "[supervisor processes]"
ps -Ao pid,ppid,command | grep -E 'codex/orchestrator/supervisor\.sh' | grep -v grep || echo "none"
echo

echo "[latest state]"
if [[ -f "$CURRENT_STATE_FILE" ]]; then
  cat "$CURRENT_STATE_FILE"
else
  echo "missing: $CURRENT_STATE_FILE"
fi
