#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="/Users/seong-useog/codex_browser/clever"
BUILD_DIR="$ROOT_DIR/build"
APP_PATH="$BUILD_DIR/src/shell/clever_browser.app"

if [ ! -d "$ROOT_DIR" ]; then
  echo "Error: project dir not found: $ROOT_DIR" >&2
  exit 1
fi

cd "$ROOT_DIR"

cmake -S . -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j "$(sysctl -n hw.ncpu)"

if [ -d "$APP_PATH" ]; then
  open "$APP_PATH"
  echo "Launched: $APP_PATH"
else
  echo "Build finished, but app bundle not found at: $APP_PATH" >&2
  exit 1
fi
