#!/usr/bin/env sh
set -e

ROOT_DIR="/Users/seong-useog/vibrowser/clever"
BUILD_DIR="$ROOT_DIR/build"
APP="$BUILD_DIR/src/shell/clever_browser.app"
BIN="$APP/Contents/MacOS/clever_browser"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j "$(sysctl -n hw.ncpu)"

"$BIN"
