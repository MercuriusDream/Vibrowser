#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ALLOW_KNOWN="${CODEX_ALLOW_KNOWN_BASELINE_FAILURES:-0}"
BASELINE_FILE="${CODEX_BASELINE_FAILURE_FILE:-$ROOT_DIR/.codex/orchestrator/known-baseline-failures.txt}"

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <ctest args...>" >&2
  exit 1
fi

tmp_output="$(mktemp)"
trap 'rm -f "$tmp_output"' EXIT

set +e
ctest "$@" >"$tmp_output" 2>&1
status=$?
set -e

cat "$tmp_output"

if [[ $status -eq 0 ]]; then
  exit 0
fi

if [[ "$ALLOW_KNOWN" != "1" || ! -f "$BASELINE_FILE" ]]; then
  exit $status
fi

actual_failures="$(grep -oE '^\[  FAILED  \] [^ ]+$' "$tmp_output" | sed 's/^\[  FAILED  \] //' | sort -u)"
baseline_failures="$(sort -u "$BASELINE_FILE")"

if [[ -z "$actual_failures" ]]; then
  exit $status
fi

unexpected="$(comm -23 <(printf "%s\n" "$actual_failures") <(printf "%s\n" "$baseline_failures"))"

if [[ -n "$unexpected" ]]; then
  echo "Unexpected test failures beyond known baseline:" >&2
  printf '%s\n' "$unexpected" >&2
  exit $status
fi

echo "Only known baseline failures detected; treating ctest result as pass for unattended estate mode."
