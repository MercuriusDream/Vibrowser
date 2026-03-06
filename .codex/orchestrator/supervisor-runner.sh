#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=.codex/orchestrator/lib.sh
source "$SCRIPT_DIR/lib.sh"

RESTART_DELAY="${CODEX_ESTATE_RESTART_DELAY:-5}"
SUPERVISOR_ARGS="${CODEX_ESTATE_SUPERVISOR_ARGS:-}"
supervisor_pid=""

stop_children() {
  if [[ -n "$supervisor_pid" ]]; then
    kill "$supervisor_pid" >/dev/null 2>&1 || true
    wait "$supervisor_pid" 2>/dev/null || true
    supervisor_pid=""
  fi
  pkill -P "$$" >/dev/null 2>&1 || true
}

on_signal() {
  touch "$STOP_FILE"
  stop_children
  exit 0
}

trap 'on_signal' INT TERM
trap 'stop_children' EXIT

while true; do
  if [[ -f "$STOP_FILE" ]]; then
    echo "Stop file detected: $STOP_FILE"
    exit 0
  fi

  bash "$SCRIPT_DIR/supervisor.sh" $SUPERVISOR_ARGS &
  supervisor_pid=$!
  wait "$supervisor_pid"
  status=$?
  supervisor_pid=""

  if [[ -f "$STOP_FILE" ]]; then
    exit 0
  fi

  echo "Supervisor exited with status $status; restarting in ${RESTART_DELAY}s." >&2
  sleep "$RESTART_DELAY"
done
