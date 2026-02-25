#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
EXAMPLES_DIR="${ROOT_DIR}/examples"
OUTPUT_FILE="${ROOT_DIR}/out.ppm"
LOCAL_PATH_OUTPUT_FILE="${ROOT_DIR}/out_local_path.ppm"
FILE_OUTPUT_FILE="${ROOT_DIR}/out_file.ppm"
SERVER_PID=""

cleanup() {
  if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
}

trap cleanup EXIT

mkdir -p "${BUILD_DIR}"

if command -v cmake >/dev/null 2>&1; then
  cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
  cmake --build "${BUILD_DIR}"
else
  SRC_FILES=()
  while IFS= read -r src_file; do
    SRC_FILES+=("${src_file}")
  done < <(find "${ROOT_DIR}/src" -type f -name "*.cpp" | sort)
  if [[ "${#SRC_FILES[@]}" -eq 0 ]]; then
    echo "No C++ sources found under src/ for fallback compile." >&2
    exit 1
  fi
  c++ -std=c++17 -I"${ROOT_DIR}/include" "${SRC_FILES[@]}" -o "${BUILD_DIR}/from_scratch_browser"
fi

BINARY=""
for candidate in \
  "${BUILD_DIR}/from_scratch_browser" \
  "${BUILD_DIR}/Debug/from_scratch_browser" \
  "${BUILD_DIR}/Release/from_scratch_browser" \
  "${BUILD_DIR}/RelWithDebInfo/from_scratch_browser"; do
  if [[ -x "${candidate}" ]]; then
    BINARY="${candidate}"
    break
  fi
done

if [[ -z "${BINARY}" ]]; then
  echo "Could not locate built executable (expected from_scratch_browser)." >&2
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required to host sample files and create file:// URLs." >&2
  exit 1
fi

render_sample() {
  local url="$1"
  local output_file="$2"
  rm -f "${output_file}"

  if "${BINARY}" "${url}" "${output_file}" >/dev/null 2>&1 && [[ -s "${output_file}" ]]; then
    return 0
  fi
  if "${BINARY}" --url "${url}" --output "${output_file}" >/dev/null 2>&1 && [[ -s "${output_file}" ]]; then
    return 0
  fi
  if "${BINARY}" "${url}" --output "${output_file}" >/dev/null 2>&1 && [[ -s "${output_file}" ]]; then
    return 0
  fi

  return 1
}

PORT=$((18000 + RANDOM % 20000))
python3 -m http.server "${PORT}" --bind 127.0.0.1 --directory "${EXAMPLES_DIR}" >/dev/null 2>&1 &
SERVER_PID=$!
sleep 1

HTTP_URL="http://127.0.0.1:${PORT}/sample.html"
if ! render_sample "${HTTP_URL}" "${OUTPUT_FILE}"; then
  echo "Failed to render ${HTTP_URL} into ${OUTPUT_FILE}." >&2
  echo "Tried: '<bin> <url> <out>', '<bin> --url <url> --output <out>', '<bin> <url> --output <out>'." >&2
  exit 1
fi
echo "Rendered HTTP output: ${OUTPUT_FILE}"

SAMPLE_HTML_PATH="${EXAMPLES_DIR}/sample.html"
if ! render_sample "${SAMPLE_HTML_PATH}" "${LOCAL_PATH_OUTPUT_FILE}"; then
  echo "Failed to render local path ${SAMPLE_HTML_PATH} into ${LOCAL_PATH_OUTPUT_FILE}." >&2
  echo "Tried: '<bin> <url> <out>', '<bin> --url <url> --output <out>', '<bin> <url> --output <out>'." >&2
  exit 1
fi
echo "Rendered local path output: ${LOCAL_PATH_OUTPUT_FILE}"

FILE_URL="$(python3 - "${SAMPLE_HTML_PATH}" <<'PY'
from pathlib import Path
import sys
print(Path(sys.argv[1]).resolve().as_uri())
PY
)"

if ! render_sample "${FILE_URL}" "${FILE_OUTPUT_FILE}"; then
  echo "Failed to render ${FILE_URL} into ${FILE_OUTPUT_FILE}." >&2
  echo "Tried: '<bin> <url> <out>', '<bin> --url <url> --output <out>', '<bin> <url> --output <out>'." >&2
  exit 1
fi

echo "Rendered file URL output: ${FILE_OUTPUT_FILE}"
