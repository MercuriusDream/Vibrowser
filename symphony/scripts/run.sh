#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [[ -f .env.symphony ]]; then
  # shellcheck disable=SC1091
  source .env.symphony
fi

if [[ ! -f WORKFLOW.md ]]; then
  echo "WORKFLOW.md is missing. Run ./scripts/setup.sh first." >&2
  exit 1
fi

if [[ -z "${LINEAR_API_KEY:-}" ]]; then
  echo "LINEAR_API_KEY is not set. Run ./scripts/setup.sh first." >&2
  exit 1
fi

exec python3 -m symphony_service.cli ./WORKFLOW.md "$@"
