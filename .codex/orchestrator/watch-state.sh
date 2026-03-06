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
  current_cycle=""
  current_dir=""
  if [[ -f "$CURRENT_STATE_FILE" ]]; then
    current_cycle="$(jq -r '.cycle // empty' "$CURRENT_STATE_FILE" 2>/dev/null || true)"
  fi
  if [[ -n "$current_cycle" ]]; then
    current_dir="$RUNS_DIR/cycle-$current_cycle"
  fi
  if [[ -n "$current_dir" && -d "$current_dir" ]]; then
    echo "Current run dir:"
    echo "$current_dir"
    echo

    for artifact in plan.json integrator.json verifier.json git-pr.json ci.json fixer.json ci-fixer.json; do
      if [[ -f "$current_dir/$artifact" ]]; then
        echo "[$artifact]"
        jq . "$current_dir/$artifact" 2>/dev/null || cat "$current_dir/$artifact"
        echo
      fi
    done

    for log_name in verifier.log ci.log ci-fixer.log git-pr.log; do
      if [[ -f "$current_dir/$log_name" ]]; then
        echo "[$log_name tail]"
        tail -n 20 "$current_dir/$log_name" || true
        echo
      fi
    done
  fi

  echo
  echo "Recent runs:"
  ls -1dt "$RUNS_DIR"/cycle-* 2>/dev/null | head -n 8 || true
  sleep 2
done
