#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=.codex/orchestrator/lib.sh
source "$SCRIPT_DIR/lib.sh"

if command -v tmux >/dev/null 2>&1; then
  tmux kill-session -t "$TMUX_SESSION" 2>/dev/null || true
  while read -r name; do
    [[ -n "$name" ]] || continue
    tmux kill-session -t "$name" 2>/dev/null || true
  done < <(tmux list-sessions 2>/dev/null | awk -F: -v prefix="${TMUX_SESSION}-" '$1 ~ "^" prefix {print $1}' || true)
fi

touch "$STOP_FILE"
echo "Requested stop for Codex estate orchestration."
