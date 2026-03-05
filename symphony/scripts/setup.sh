#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required but not found." >&2
  exit 1
fi

if [[ ! -f WORKFLOW.md ]]; then
  cp WORKFLOW.example.md WORKFLOW.md
  echo "Created WORKFLOW.md from WORKFLOW.example.md"
fi

if [[ -z "${LINEAR_PROJECT_SLUG:-}" ]]; then
  read -r -p "Linear project slug (e.g. eng): " LINEAR_PROJECT_SLUG
fi

if [[ -z "${LINEAR_API_KEY:-}" ]]; then
  read -r -s -p "Linear personal API key: " LINEAR_API_KEY
  echo
fi

if [[ -z "$LINEAR_PROJECT_SLUG" ]]; then
  echo "LINEAR_PROJECT_SLUG is required." >&2
  exit 1
fi

if [[ -z "$LINEAR_API_KEY" ]]; then
  echo "LINEAR_API_KEY is required." >&2
  exit 1
fi

{
  printf "export LINEAR_API_KEY=%q\n" "$LINEAR_API_KEY"
  printf "export LINEAR_PROJECT_SLUG=%q\n" "$LINEAR_PROJECT_SLUG"
} > .env.symphony
chmod 600 .env.symphony

echo "Saved credentials to .env.symphony"

if command -v rg >/dev/null 2>&1 && rg -q "project_slug:\s*your-project-slug" WORKFLOW.md; then
  sed -i '' "s/project_slug:[[:space:]]*your-project-slug/project_slug: ${LINEAR_PROJECT_SLUG}/" WORKFLOW.md
  echo "Updated WORKFLOW.md project_slug"
elif command -v rg >/dev/null 2>&1 && rg -q "project_slug:\s*demo" WORKFLOW.md; then
  sed -i '' "s/project_slug:[[:space:]]*demo/project_slug: ${LINEAR_PROJECT_SLUG}/" WORKFLOW.md
  echo "Updated WORKFLOW.md project_slug"
else
  echo "Did not auto-update project_slug in WORKFLOW.md. Set tracker.project_slug manually if needed."
fi

echo
echo "Setup complete."
echo "Next: ./scripts/run.sh"
