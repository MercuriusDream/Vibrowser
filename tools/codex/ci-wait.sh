#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

TIMEOUT_SECONDS="${CODEX_ESTATE_CI_TIMEOUT_SECONDS:-3600}"
INTERVAL_SECONDS="${CODEX_ESTATE_CI_POLL_INTERVAL:-30}"
deadline=$(( $(date +%s) + TIMEOUT_SECONDS ))
BRANCH=""
PR_URL=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --branch)
      BRANCH="$2"
      shift
      ;;
    --pr-url)
      PR_URL="$2"
      shift
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1
      ;;
  esac
  shift
done

if ! gh auth status >/dev/null 2>&1; then
  jq -n '{status:"gh_not_authenticated"}'
  exit 2
fi

if [[ -z "$BRANCH" ]]; then
  BRANCH="$(git rev-parse --abbrev-ref HEAD)"
fi

while true; do
  if [[ -n "$PR_URL" ]]; then
    pr_json="$(gh pr view "$PR_URL" --json number,url,statusCheckRollup,headRefName 2>/dev/null || true)"
  else
    pr_json="$(gh pr view --head "$BRANCH" --json number,url,statusCheckRollup,headRefName 2>/dev/null || true)"
  fi
  if [[ -z "$pr_json" ]]; then
    jq -n --arg branch "$BRANCH" --arg pr_url "$PR_URL" '{status:"no_pr",branch:$branch,pr_url:$pr_url}'
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
