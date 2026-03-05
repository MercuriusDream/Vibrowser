#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if ! gh auth status >/dev/null 2>&1; then
  jq -n '{status:"gh_not_authenticated"}'
  exit 0
fi

branch="$(git rev-parse --abbrev-ref HEAD)"
pr_json="$(gh pr view --head "$branch" --json number,url,statusCheckRollup 2>/dev/null || true)"

if [[ -z "$pr_json" ]]; then
  jq -n --arg branch "$branch" '{status:"no_pr",branch:$branch}'
  exit 0
fi

printf '%s\n' "$pr_json"
