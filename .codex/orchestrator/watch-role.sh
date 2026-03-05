#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=.codex/orchestrator/lib.sh
source "$SCRIPT_DIR/lib.sh"

role="${1:-worker-1}"

while true; do
  latest="$(ls -1t "$RUNS_DIR"/cycle-*"/${role}.log" 2>/dev/null | head -n 1 || true)"
  clear
  echo "Role: $role"
  echo "Project: $PROJECT_DIR"
  if [[ -n "$latest" && -f "$latest" ]]; then
    echo "Log: $latest"
    echo
    tail -n 200 -F "$latest"
  else
    echo "Waiting for log file..."
    sleep 2
  fi
done
