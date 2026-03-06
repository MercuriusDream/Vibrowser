#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APP_ROOT="$ROOT_DIR/vibrowser"
BUILD_DIR="${CODEX_BUILD_DIR:-$APP_ROOT/build_estate}"

bash "$ROOT_DIR/tools/codex/build.sh"

if ctest --test-dir "$BUILD_DIR" -N 2>/dev/null | rg -qi 'vibrowser_(paint|layout|css_style|js|net|html)_tests'; then
  bash "$ROOT_DIR/tools/codex/ctest-with-baseline.sh" --test-dir "$BUILD_DIR" --output-on-failure -R 'vibrowser_(paint|layout|css_style|js|net|html)_tests'
else
  echo "No curated smoke test regex matched; skipping."
fi
