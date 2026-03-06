#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

CYCLE=""
SUMMARY=""
PUSH="${CODEX_ESTATE_PUSH:-0}"
CREATE_PR="${CODEX_ESTATE_CREATE_PR:-0}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --cycle)
      CYCLE="$2"
      shift
      ;;
    --summary)
      SUMMARY="$2"
      shift
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1
      ;;
  esac
  shift
done

if [[ -z "$CYCLE" ]]; then
  echo "Missing --cycle" >&2
  exit 1
fi

if [[ -z "$SUMMARY" ]]; then
  SUMMARY="codex estate cycle $CYCLE"
fi

current_branch="$(git rev-parse --abbrev-ref HEAD)"
branch_pattern="codex/estate-cycle-${CYCLE}-"
existing_branch="$(git branch --list "${branch_pattern}*" | sed 's/^[*[:space:]]*//' | head -n 1 || true)"
branch="codex/estate-cycle-${CYCLE}-$(date '+%Y%m%d-%H%M%S')"
short_summary="$(printf '%s' "$SUMMARY" | cut -c1-72)"

if git diff --quiet && git diff --cached --quiet; then
  jq -n --arg branch "" --arg summary "No changes to commit" '{status:"no_changes",branch:$branch,summary:$summary,pr_url:""}'
  exit 0
fi

if [[ "$current_branch" == ${branch_pattern}* ]]; then
  branch="$current_branch"
elif [[ -n "$existing_branch" ]]; then
  branch="$existing_branch"
  git checkout "$branch" >&2
else
  git checkout -b "$branch" >&2
fi

git add -A

if git diff --cached --quiet; then
  jq -n --arg branch "$branch" --arg summary "Nothing staged after git add" '{status:"no_staged_changes",branch:$branch,summary:$summary,pr_url:""}'
  exit 0
fi

git commit -m "estate: cycle $CYCLE $short_summary" >&2

pr_url=""
if [[ "$PUSH" == "1" ]]; then
  git push -u origin "$branch" >&2
  if [[ "$CREATE_PR" == "1" ]]; then
    pr_url="$(gh pr view --head "$branch" --json url -q .url 2>/dev/null || true)"
    if [[ -z "$pr_url" ]]; then
      pr_url="$(gh pr create --fill --base main --head "$branch" 2>/dev/null || true)"
    fi
  fi
fi

jq -n \
  --arg branch "$branch" \
  --arg summary "$SUMMARY" \
  --arg pr_url "$pr_url" \
  '{status:"prepared",branch:$branch,summary:$summary,pr_url:$pr_url}'
