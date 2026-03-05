#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APP_ROOT="$ROOT_DIR/vibrowser"
BUILD_DIR="${CODEX_BUILD_DIR:-$APP_ROOT/build_estate}"

bash "$ROOT_DIR/tools/codex/build.sh"

if ctest --test-dir "$BUILD_DIR" -N 2>/dev/null | rg -qi 'bench|perf|benchmark'; then
  ctest --test-dir "$BUILD_DIR" --output-on-failure -R 'bench|perf|benchmark'
else
  echo "No benchmark/perf ctest targets detected; skipping."
fi
