#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

TIMEOUT_SECONDS="${CODEX_ESTATE_CI_TIMEOUT_SECONDS:-3600}"
INTERVAL_SECONDS="${CODEX_ESTATE_CI_POLL_INTERVAL:-30}"
deadline=$(( $(date +%s) + TIMEOUT_SECONDS ))

if ! gh auth status >/dev/null 2>&1; then
  jq -n '{status:"gh_not_authenticated"}'
  exit 2
fi

branch="$(git rev-parse --abbrev-ref HEAD)"

while true; do
  pr_json="$(gh pr view --head "$branch" --json number,url,statusCheckRollup 2>/dev/null || true)"
  if [[ -z "$pr_json" ]]; then
    jq -n --arg branch "$branch" '{status:"no_pr",branch:$branch}'
    exit 2
  fi

  total="$(jq '.statusCheckRollup | length' <<<"$pr_json")"
  pending="$(jq '[.statusCheckRollup[] | select((.conclusion == null) or (.status == "IN_PROGRESS") or (.status == "QUEUED") or (.status == "PENDING"))] | length' <<<"$pr_json")"
  failed="$(jq '[.statusCheckRollup[] | select(.conclusion == "FAILURE" or .conclusion == "TIMED_OUT" or .conclusion == "CANCELLED" or .conclusion == "ACTION_REQUIRED" or .conclusion == "STARTUP_FAILURE")] | length' <<<"$pr_json")"

  if [[ "$total" -eq 0 ]]; then
    jq -n --argjson pr "$pr_json" '{status:"no_checks",pr:$pr}'
    exit 2
  fi

  if [[ "$failed" -gt 0 ]]; then
    jq -n --argjson pr "$pr_json" '{status:"failed",pr:$pr}'
    exit 1
  fi

  if [[ "$pending" -eq 0 ]]; then
    jq -n --argjson pr "$pr_json" '{status:"passed",pr:$pr}'
    exit 0
  fi

  if [[ "$(date +%s)" -ge "$deadline" ]]; then
    jq -n --argjson pr "$pr_json" '{status:"timeout",pr:$pr}'
    exit 2
  fi

  sleep "$INTERVAL_SECONDS"
done
