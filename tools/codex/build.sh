#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APP_ROOT="$ROOT_DIR/vibrowser"
BUILD_DIR="${CODEX_BUILD_DIR:-$APP_ROOT/build_estate}"

cmake -S "$APP_ROOT" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j "$(sysctl -n hw.ncpu)"
