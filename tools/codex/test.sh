#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APP_ROOT="$ROOT_DIR/vibrowser"
BUILD_DIR="${CODEX_BUILD_DIR:-$APP_ROOT/build_estate}"

bash "$ROOT_DIR/tools/codex/build.sh"
ctest --test-dir "$BUILD_DIR" --output-on-failure "$@"
