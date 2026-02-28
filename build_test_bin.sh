#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

APP_ROOT=""
for candidate in "${SCRIPT_DIR}/vibrowser"; do
  if [ -d "${candidate}" ]; then
    APP_ROOT="${candidate}"
    break
  fi
done

if [ -z "${APP_ROOT}" ]; then
  echo "Error: could not find app module directory (expected ./vibrowser)." >&2
  exit 1
fi

BUILD_DIR="${APP_ROOT}/build"

cmake -S "${APP_ROOT}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"

BIN_PATH=""
for candidate in \
  "${BUILD_DIR}/src/shell/vibrowser.app/Contents/MacOS/vibrowser"; do
  if [ -x "${candidate}" ]; then
    BIN_PATH="${candidate}"
    break
  fi
done

if [ -z "${BIN_PATH}" ]; then
  echo "Build finished, but app binary not found under: ${BUILD_DIR}/src/shell" >&2
  exit 1
fi

"${BIN_PATH}"
