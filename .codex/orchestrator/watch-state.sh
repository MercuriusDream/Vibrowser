#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=.codex/orchestrator/lib.sh
source "$SCRIPT_DIR/lib.sh"

while true; do
  clear
  echo "Codex Estate State"
  echo
  if [[ -f "$CURRENT_STATE_FILE" ]]; then
    jq . "$CURRENT_STATE_FILE"
  else
    echo "No current state file."
  fi
  echo
  echo "Recent runs:"
  ls -1dt "$RUNS_DIR"/cycle-* 2>/dev/null | head -n 8 || true
  sleep 2
done
