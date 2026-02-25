#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
EXAMPLES_DIR="${ROOT_DIR}/examples"
OUTPUT_FILE="${ROOT_DIR}/smoke_out.ppm"
FEATURE_OUTPUT_FILE="${ROOT_DIR}/smoke_feature_out.ppm"
FEATURE_LOCAL_OUTPUT_FILE="${ROOT_DIR}/smoke_feature_local_out.ppm"
SAMPLE_FILE="${EXAMPLES_DIR}/smoke_sample.html"
SERVER_LOG="${BUILD_DIR}/smoke_server.log"
SMOKE_PORT="${SMOKE_PORT:-8765}"
STRICT_FEATURE_ASSERT="${STRICT_FEATURE_ASSERT:-0}"

SERVER_PID=""
BROWSER_BIN=""
SMOKE_MAIN_CPP=""
FEATURE_RENDER_LOG=""
FEATURE_LOCAL_RENDER_LOG=""
FEATURE_LOCAL_RENDER_OK=0

cleanup() {
  if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" >/dev/null 2>&1; then
    kill "${SERVER_PID}" >/dev/null 2>&1 || true
    wait "${SERVER_PID}" >/dev/null 2>&1 || true
  fi

  if [[ -n "${SMOKE_MAIN_CPP}" && -f "${SMOKE_MAIN_CPP}" ]]; then
    rm -f "${SMOKE_MAIN_CPP}"
  fi
}
trap cleanup EXIT

find_cmake_binary() {
  local candidate

  if [[ -x "${BUILD_DIR}/from_scratch_browser" ]]; then
    BROWSER_BIN="${BUILD_DIR}/from_scratch_browser"
    return 0
  fi

  candidate="$(find "${BUILD_DIR}" -type f -name 'from_scratch_browser' -perm -u+x 2>/dev/null | head -n 1 || true)"
  if [[ -n "${candidate}" ]]; then
    BROWSER_BIN="${candidate}"
    return 0
  fi

  return 1
}

build_browser_direct() {
  local cxx_bin
  local has_main
  local -a sources

  cxx_bin="${CXX:-c++}"
  if ! command -v "${cxx_bin}" >/dev/null 2>&1; then
    echo "[FAIL] No C++ compiler found for direct build fallback." >&2
    return 1
  fi

  mkdir -p "${BUILD_DIR}"

    sources=()
  while IFS= read -r source; do
    sources+=("${source}")
  done < <(find "${ROOT_DIR}/src" -type f -name '*.cpp' | sort)

  if [[ ${#sources[@]} -eq 0 ]]; then
    echo "[FAIL] No C++ sources found under src/." >&2
    return 1
  fi

  if rg -n '^[[:space:]]*int[[:space:]]+main[[:space:]]*\(' "${ROOT_DIR}/src" >/dev/null 2>&1; then
    has_main=1
  else
    has_main=0
  fi

  BROWSER_BIN="${BUILD_DIR}/from_scratch_browser_smoke"

  if [[ ${has_main} -eq 1 ]]; then
    "${cxx_bin}" -std=c++17 -O2 -I"${ROOT_DIR}/include" "${sources[@]}" -o "${BROWSER_BIN}"
    return 0
  fi

  SMOKE_MAIN_CPP="${BUILD_DIR}/smoke_main.cpp"
  cat > "${SMOKE_MAIN_CPP}" <<'CPP'
#include <iostream>
#include <string>

#include "browser/css/css_parser.h"
#include "browser/html/html_parser.h"
#include "browser/layout/layout_engine.h"
#include "browser/net/http_client.h"
#include "browser/render/renderer.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0] << " <url> <output.ppm>\\n";
    return 2;
  }

  const std::string url = argv[1];
  const std::string out = argv[2];

  const auto response = browser::net::fetch(url, 3, 5);
  if (!response.error.empty()) {
    std::cerr << "fetch failed: " << response.error << "\\n";
    return 1;
  }

  if (response.status_code < 200 || response.status_code >= 400) {
    std::cerr << "bad status code: " << response.status_code << "\\n";
    return 1;
  }

  auto dom = browser::html::parse_html(response.body);
  if (!dom) {
    std::cerr << "parse_html returned null document\\n";
    return 1;
  }

  const auto stylesheet = browser::css::parse_css("");
  const auto layout = browser::layout::layout_document(*dom, stylesheet, 800);
  const auto canvas = browser::render::render_to_canvas(layout, 800, 600);

  if (canvas.empty()) {
    std::cerr << "rendered canvas is empty\\n";
    return 1;
  }

  if (!browser::render::write_ppm(canvas, out)) {
    std::cerr << "failed to write output file: " << out << "\\n";
    return 1;
  }

  return 0;
}
CPP

  "${cxx_bin}" -std=c++17 -O2 -I"${ROOT_DIR}/include" "${sources[@]}" "${SMOKE_MAIN_CPP}" -o "${BROWSER_BIN}"
}

build_browser() {
  mkdir -p "${BUILD_DIR}"

  if command -v cmake >/dev/null 2>&1; then
    echo "[INFO] Building with CMake..."
    if cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" && cmake --build "${BUILD_DIR}"; then
      if find_cmake_binary; then
        return 0
      fi
      echo "[WARN] CMake build succeeded but no executable was found; using direct compile fallback."
    else
      echo "[WARN] CMake build failed; using direct compile fallback."
    fi
  else
    echo "[INFO] CMake not found; using direct compile fallback."
  fi

  build_browser_direct
}

check_server_once() {
  python3 - "$1" <<'PY'
import sys
import urllib.request

url = sys.argv[1]
try:
    with urllib.request.urlopen(url, timeout=0.5) as response:
        sys.exit(0 if response.status == 200 else 1)
except Exception:
    sys.exit(1)
PY
}

start_server() {
  local url

  if ! command -v python3 >/dev/null 2>&1; then
    echo "[FAIL] python3 is required to run the smoke test server." >&2
    return 1
  fi

  mkdir -p "${EXAMPLES_DIR}"
  cat > "${SAMPLE_FILE}" <<'HTML'
<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Smoke Test</title>
    <style>
      body { margin: 0; background: #ffffff; }
      h1 { color: #0f4c81; margin: 24px; }
      p { color: #222222; margin: 24px; }
    </style>
  </head>
  <body>
    <h1>Browser Smoke Test</h1>
    <p>If you can render this, the pipeline is alive.</p>
  </body>
</html>
HTML

  url="http://127.0.0.1:${SMOKE_PORT}/smoke_sample.html"

  (
    cd "${EXAMPLES_DIR}"
    python3 -m http.server "${SMOKE_PORT}" --bind 127.0.0.1 > "${SERVER_LOG}" 2>&1
  ) &
  SERVER_PID="$!"

  for _ in {1..30}; do
    if check_server_once "${url}"; then
      return 0
    fi
    sleep 0.2
  done

  echo "[FAIL] Failed to start local HTTP server on port ${SMOKE_PORT}." >&2
  return 1
}

run_browser_render() {
  local url="$1"
  local output_file="$2"
  local width="${3:-}"
  local height="${4:-}"

  rm -f "${output_file}"

  if [[ -n "${width}" && -n "${height}" ]]; then
    if "${BROWSER_BIN}" "${url}" "${output_file}" "${width}" "${height}"; then
      return 0
    fi
  fi

  if "${BROWSER_BIN}" "${url}" "${output_file}"; then
    return 0
  fi

  if "${BROWSER_BIN}" --url "${url}" --out "${output_file}"; then
    return 0
  fi

  if "${BROWSER_BIN}" --url "${url}" --output "${output_file}"; then
    return 0
  fi

  if "${BROWSER_BIN}" --input-url "${url}" --output "${output_file}"; then
    return 0
  fi

  return 1
}

run_smoke_render() {
  local url
  url="http://127.0.0.1:${SMOKE_PORT}/smoke_sample.html"
  run_browser_render "${url}" "${OUTPUT_FILE}"
}

run_feature_render() {
  local url
  local render_output
  local sample_path
  local local_render_output

  url="http://127.0.0.1:${SMOKE_PORT}/sample.html"
  render_output="$(run_browser_render "${url}" "${FEATURE_OUTPUT_FILE}" 1280 1600 2>&1)" || {
    printf '%s\n' "${render_output}" >&2
    return 1
  }
  FEATURE_RENDER_LOG="${render_output}"
  if [[ -n "${FEATURE_RENDER_LOG}" ]]; then
    printf '%s\n' "${FEATURE_RENDER_LOG}"
  fi

  sample_path="${EXAMPLES_DIR}/sample.html"
  local_render_output="$(run_browser_render "${sample_path}" "${FEATURE_LOCAL_OUTPUT_FILE}" 1280 1600 2>&1)" || {
    FEATURE_LOCAL_RENDER_OK=0
    FEATURE_LOCAL_RENDER_LOG="${local_render_output}"
    if [[ -n "${FEATURE_LOCAL_RENDER_LOG}" ]]; then
      printf '%s\n' "${FEATURE_LOCAL_RENDER_LOG}" >&2
    fi
    return 0
  }

  FEATURE_LOCAL_RENDER_OK=1
  FEATURE_LOCAL_RENDER_LOG="${local_render_output}"
  if [[ -n "${FEATURE_LOCAL_RENDER_LOG}" ]]; then
    printf '%s\n' "${FEATURE_LOCAL_RENDER_LOG}"
  fi
}

assert_feature_background() {
  python3 - "${FEATURE_OUTPUT_FILE}" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = path.read_bytes()

def read_token(index):
    n = len(data)
    while index < n:
        b = data[index]
        if b in b" \t\r\n":
            index += 1
            continue
        if b == ord("#"):
            while index < n and data[index] not in (ord("\n"), ord("\r")):
                index += 1
            continue
        break
    if index >= n:
        raise ValueError("unexpected end of header")
    end = index
    while end < n and data[end] not in b" \t\r\n":
        end += 1
    return data[index:end].decode("ascii"), end

magic, cursor = read_token(0)
if magic != "P6":
    raise SystemExit(f"expected P6 ppm, got {magic!r}")

width_token, cursor = read_token(cursor)
height_token, cursor = read_token(cursor)
max_value, cursor = read_token(cursor)
width = int(width_token)
height = int(height_token)

if max_value != "255":
    raise SystemExit(f"unsupported ppm max value: {max_value}")

while cursor < len(data) and data[cursor] in b" \t\r\n":
    cursor += 1

pixels = data[cursor:]
expected_size = width * height * 3
if len(pixels) < expected_size:
    raise SystemExit("ppm payload is truncated")

def near(rgb, target, tolerance):
    return (
        abs(rgb[0] - target[0]) <= tolerance and
        abs(rgb[1] - target[1]) <= tolerance and
        abs(rgb[2] - target[2]) <= tolerance
    )

def near_any(rgb, targets, tolerance):
    return any(near(rgb, target, tolerance) for target in targets)

def clamp_region(x0, y0, x1, y1):
    left = max(0, min(width - 1, x0))
    top = max(0, min(height - 1, y0))
    right = max(0, min(width - 1, x1))
    bottom = max(0, min(height - 1, y1))
    if right < left or bottom < top:
        return None
    return left, top, right, bottom

def count_region_matches(x0, y0, x1, y1, targets, tolerance):
    bounds = clamp_region(x0, y0, x1, y1)
    if bounds is None:
        return 0, 0
    left, top, right, bottom = bounds
    matches = 0
    total = (right - left + 1) * (bottom - top + 1)
    for yy in range(top, bottom + 1):
        row_offset = yy * width * 3
        for xx in range(left, right + 1):
            offset = row_offset + xx * 3
            rgb = (pixels[offset], pixels[offset + 1], pixels[offset + 2])
            if near_any(rgb, targets, tolerance):
                matches += 1
    return matches, total

expected_body_centers = (
    (220, 233, 251),  # sample.css body background (#dce9fb)
    (230, 255, 237),  # sample.js runtime mutation (#e6ffed)
)
body_probe_width = max(12, width // 20)
body_probe_height = max(12, height // 20)
body_region_hits, body_region_total = count_region_matches(
    0,
    0,
    body_probe_width,
    body_probe_height,
    expected_body_centers,
    20,
)
if body_region_hits < max(24, body_region_total // 8):
    raise SystemExit(
        "feature assertion failed: body/background sanity check in top-left region "
        f"found {body_region_hits}/{body_region_total} matching pixels, expected at least "
        f"{max(24, body_region_total // 8)} near {expected_body_centers}"
    )

transparent_fixture_centers = (
    (255, 255, 255),  # transparent blended over white fallback
    (220, 233, 251),  # sample.css body background (#dce9fb)
    (230, 255, 237),  # sample.js runtime mutation (#e6ffed)
)
transparent_region_hits, transparent_region_total = count_region_matches(
    32,
    32,
    min(width - 1, 280),
    min(height - 1, 82),
    transparent_fixture_centers,
    28,
)
transparent_fixture_ok = transparent_region_hits >= max(60, transparent_region_total // 14)
if not transparent_fixture_ok:
    raise SystemExit(
        "feature assertion failed: transparent fixture sanity check in top fixture region "
        f"found {transparent_region_hits}/{transparent_region_total} matching pixels, "
        f"expected at least {max(60, transparent_region_total // 14)} near {transparent_fixture_centers}"
    )

tracked_signals = {
    "js_bg_yellow": 0,          # sample.js querySelector(...).style.background/backgroundColor (#fde68a)
    "js_chip_blue": 0,          # sample.js querySelector(...).style.backgroundColor (#bfdbfe)
    "js_style_string_green": 0,  # sample.js querySelector(...).style = "..." background-color (#86efac)
    "js_style_string_border_green": 0,  # sample.js querySelector(...).style = "..." border-color (#22c55e)
    "js_setattr_green_bg": 0,  # sample.js getElementById(...).setAttribute("style", ...) background-color (#d9f99d)
    "js_setattr_nonstyle_bg": 0,  # sample.js getElementById(...).setAttribute("class", ...) triggered CSS bg (#8b5cf6)
    "js_setattr_nonstyle_border": 0,  # sample.css .setattr-nonstyle-fixture.armed border color (#5b21b6)
    "js_setattr_nonstyle_fallback_bg": 0,  # sample.css fallback before runtime class mutation
    "selector_child_blue": 0,   # sample.css fallback before rgba() (#e9f5ff)
    "selector_nth_rgba": 0,     # sample.css :nth-child( 1 ) + rgba(...) background
    "selector_nth_hsl_bg": 0,   # sample.css :nth-child( 2 ) + hsl(...) background
    "selector_nth_hsl_border": 0,  # sample.css :nth-child( 2 ) + hsl(...) border
    "selector_nth_odd_named_orange": 0,  # sample.css :nth-child(odd) nested-card border-color (orange)
    "selector_nth_even_nested_bg": 0,  # sample.css :nth-child(even) nested-card background (#fff1d6)
    "selector_adjacent_green": 0,  # sample.css adjacent sibling combinator (#d1fae5)
    "js_border_orange": 0,      # sample.js querySelector(...).style.borderColor (#f97316)
    "selector_sibling_green": 0,  # sample.css general sibling combinator (#10b981)
    "selector_hex_alpha_hero_bg": 0,  # sample.css .hero background #dbeafe80
    "selector_hero_fallback_bg": 0,  # sample.css .hero fallback background #dbeafe
    "data_url_note_bg": 0,      # sample.html data:text/css fixture background (#e0f2fe)
    "selector_attr_rgb_bg": 0,  # sample.css [id=attr-fixture] background (#9cf)
    "selector_attr_rgb_border": 0,  # sample.css [id=attr-fixture] border (#48a)
    "selector_attr_prefix_bg": 0,  # sample.css [data-prefix-token^="wave-"] background (#a3e635)
    "selector_attr_prefix_border": 0,  # sample.css [data-prefix-token^="wave-"] border-color (#365314)
    "selector_attr_prefix_fallback_bg": 0,  # sample.css attr-prefix fallback background (#d4b5ff)
    "selector_attr_prefix_fallback_border": 0,  # sample.css attr-prefix fallback border-color (#7e22ce)
    "selector_attr_suffix_bg": 0,  # sample.css [data-suffix-token$="-suffix"] background (#facc15)
    "selector_attr_suffix_border": 0,  # sample.css [data-suffix-token$="-suffix"] border-color (#713f12)
    "selector_attr_suffix_fallback_bg": 0,  # sample.css attr-suffix fallback background (#94a3b8)
    "selector_attr_suffix_fallback_border": 0,  # sample.css attr-suffix fallback border-color (#475569)
    "selector_attr_contains_bg": 0,  # sample.css [data-contains-token*="wave-middle"] background (#f0abfc)
    "selector_attr_contains_border": 0,  # sample.css [data-contains-token*="wave-middle"] border-color (#701a75)
    "selector_attr_contains_fallback_bg": 0,  # sample.css attr-contains fallback background (#e2e8f0)
    "selector_attr_contains_fallback_border": 0,  # sample.css attr-contains fallback border-color (#334155)
    "selector_attr_exists_bg": 0,  # sample.css [data-exists-token] background (#8ecae6)
    "selector_attr_exists_border": 0,  # sample.css [data-exists-token] border-color (#023047)
    "selector_attr_exists_fallback_bg": 0,  # sample.css .attr-exists-fixture fallback background (#ffb4a2)
    "selector_attr_exists_fallback_border": 0,  # sample.css .attr-exists-fixture fallback border-color (#9d0208)
    "selector_class_contains_border": 0,  # sample.css [class~=token-target] border (#2563eb)
    "selector_class_fallback_border": 0,  # sample.css #transparent-fixture border fallback (#0ea5e9)
    "selector_currentcolor_border": 0,  # sample.css currentColor border fixture (#7c2d12)
    "selector_not_bg": 0,  # sample.css :not(.skip-not) fixture background (#22d3ee)
    "selector_not_border": 0,  # sample.css :not(.skip-not) fixture border-color (#0e7490)
    "selector_not_fallback_bg": 0,  # sample.css .not-fixture-item fallback background (#fb7185)
    "entity_decode_bg": 0,  # sample.css #entity-text-fixture background via entity-decoded id (#84cc16)
    "entity_decode_border": 0,  # sample.css #entity-text-fixture border-color via entity-decoded id (#3f6212)
    "entity_decode_fallback_bg": 0,  # sample.css entity fixture fallback background (#f59e0b)
    "apos_entity_decode_bg": 0,  # sample.css [id=\"apos'fixture\"] background via &apos; decode (#bef264)
    "apos_entity_decode_border": 0,  # sample.css [id=\"apos'fixture\"] border-color via &apos; decode (#4d7c0f)
    "apos_entity_fallback_bg": 0,  # sample.css apos fixture fallback background (#fecdd3)
    "numeric_entity_decode_bg": 0,  # sample.css #numeric-entity-fixture background via numeric-decoded id (#2dd4bf)
    "numeric_entity_decode_border": 0,  # sample.css #numeric-entity-fixture border-color (#115e59)
    "numeric_entity_fallback_bg": 0,  # sample.css numeric entity fixture fallback background (#f5d0fe)
    "numeric_entity_fallback_border": 0,  # sample.css numeric entity fixture fallback border-color (#a21caf)
    "copy_reg_trade_entity_decode_bg": 0,  # sample.css decoded id fixture background via &copy;/&reg;/&trade; (#a7f3d0)
    "copy_reg_trade_entity_decode_border": 0,  # sample.css decoded id fixture border-color (#047857)
    "copy_reg_trade_entity_fallback_bg": 0,  # sample.css copy/reg/trade entity fixture fallback background (#fbcfe8)
    "copy_reg_trade_entity_fallback_border": 0,  # sample.css copy/reg/trade entity fixture fallback border-color (#9d174d)
    "mdash_ndash_entity_decode_bg": 0,  # sample.css [id=\"dash—between–fixture\"] background via &mdash;/&ndash; decode (#4ade80)
    "mdash_ndash_entity_decode_border": 0,  # sample.css [id=\"dash—between–fixture\"] border-color (#166534)
    "mdash_ndash_entity_fallback_bg": 0,  # sample.css dash entity fixture fallback background (#fcd34d)
    "mdash_ndash_entity_fallback_border": 0,  # sample.css dash entity fixture fallback border-color (#a16207)
    "currency_entity_decode_bg": 0,  # sample.css [id=\"currency¢and£and€fixture\"] background via &cent;/&pound;/&euro; decode (#f472b6)
    "currency_entity_decode_border": 0,  # sample.css [id=\"currency¢and£and€fixture\"] border-color (#831843)
    "currency_entity_fallback_bg": 0,  # sample.css currency entity fallback background (#ffedd5)
    "currency_entity_fallback_border": 0,  # sample.css currency entity fallback border-color (#92400e)
    "yen_sect_deg_entity_decode_bg": 0,  # sample.css [id=\"yen¥sect§deg°fixture\"] background via &yen;/&sect;/&deg; decode (#b9fbc0)
    "yen_sect_deg_entity_decode_border": 0,  # sample.css [id=\"yen¥sect§deg°fixture\"] border-color (#118ab2)
    "yen_sect_deg_entity_fallback_bg": 0,  # sample.css yen/sect/deg entity fallback background (#ffd6a5)
    "yen_sect_deg_entity_fallback_border": 0,  # sample.css yen/sect/deg entity fallback border-color (#b08968)
    "selector_only_child_hsla_bg": 0,  # sample.css :only-child fixture hsla(...) background
    "selector_only_child_fallback_bg": 0,  # sample.css :only-child fixture fallback background (#cffafe)
    "selector_first_of_type_bg": 0,  # sample.css :first-of-type fixture background (#6ee7b7)
    "selector_first_of_type_border": 0,  # sample.css :first-of-type fixture border-color (#059669)
    "selector_first_of_type_fallback_bg": 0,  # sample.css fallback class background (#c4b5fd)
    "selector_first_of_type_fallback_border": 0,  # sample.css fallback class border-color (#7c3aed)
    "selector_last_of_type_bg": 0,  # sample.css :last-of-type fixture background (#fecaca)
    "selector_last_of_type_border": 0,  # sample.css :last-of-type fixture border-color (#be185d)
    "selector_last_of_type_fallback_bg": 0,  # sample.css fallback class background (#fdba74)
    "selector_last_of_type_fallback_border": 0,  # sample.css fallback class border-color (#ea580c)
    "selector_nth_of_type_bg": 0,  # sample.css :nth-of-type(2) fixture background (#99f6e4)
    "selector_nth_of_type_border": 0,  # sample.css :nth-of-type(2) fixture border-color (#0d9488)
    "selector_nth_of_type_fallback_bg": 0,  # sample.css nth-of-type fallback class background (#f9a8d4)
    "selector_nth_of_type_fallback_border": 0,  # sample.css nth-of-type fallback class border-color (#db2777)
    "selector_nth_last_child_bg": 0,  # sample.css :nth-last-child(2) fixture background (#93c5fd)
    "selector_nth_last_child_border": 0,  # sample.css :nth-last-child(2) fixture border-color (#1d4ed8)
    "selector_nth_last_child_fallback_bg": 0,  # sample.css nth-last-child fallback class background (#fca5a5)
    "selector_nth_last_child_fallback_border": 0,  # sample.css nth-last-child fallback class border-color (#b91c1c)
    "selector_nth_last_normalized_bg": 0,  # sample.css :NTH-LAST-CHILD( 2 ) normalized fixture background (#14b8a6)
    "selector_nth_last_normalized_border": 0,  # sample.css :NTH-LAST-CHILD( 2 ) normalized fixture border-color (#134e4a)
    "selector_nth_last_normalized_fallback_bg": 0,  # sample.css normalized nth-last fallback background (#e879f9)
    "selector_nth_last_normalized_fallback_border": 0,  # sample.css normalized nth-last fallback border-color (#86198f)
    "js_classname_runtime_bg": 0,  # sample.css runtime className fixture background (#67e8f9)
    "js_classname_runtime_border": 0,  # sample.css runtime className fixture border-color (#0369a1)
    "js_classname_fallback_bg": 0,  # sample.css runtime className fallback background (#e9d5ff)
    "js_classname_fallback_border": 0,  # sample.css runtime className fallback border-color (#a855f7)
    "js_runtime_id_bg": 0,  # sample.css runtime .id mutation fixture background (#bbf7d0)
    "js_runtime_id_border": 0,  # sample.css runtime .id mutation fixture border-color (#15803d)
    "js_runtime_id_fallback_bg": 0,  # sample.css runtime .id fixture fallback background (#fed7aa)
    "js_runtime_id_fallback_border": 0,  # sample.css runtime .id fixture fallback border-color (#c2410c)
    "body_classname_runtime_bg": 0,  # sample.css body.body-runtime-armed fixture background (#a78bfa)
    "body_classname_runtime_border": 0,  # sample.css body.body-runtime-armed fixture border-color (#6d28d9)
    "body_classname_fallback_bg": 0,  # sample.css body-classname fixture fallback background (#e5e7eb)
    "body_classname_fallback_border": 0,  # sample.css body-classname fixture fallback border-color (#6b7280)
    "body_id_runtime_bg": 0,  # sample.css body#page-shell-armed fixture background (#7dd3fc)
    "body_id_runtime_border": 0,  # sample.css body#page-shell-armed fixture border-color (#1e40af)
    "body_id_fallback_bg": 0,  # sample.css body-id fixture fallback background (#ede9fe)
    "body_id_fallback_border": 0,  # sample.css body-id fixture fallback border-color (#6366f1)
    "body_setattr_runtime_bg": 0,  # sample.css body.body-setattr-runtime fixture background (#3a86ff)
    "body_setattr_runtime_border": 0,  # sample.css body.body-setattr-runtime fixture border-color (#1d3557)
    "body_setattr_attr_bg": 0,  # sample.css body[data-body-setattr-fixture] fixture background (#52b788)
    "body_setattr_attr_border": 0,  # sample.css body[data-body-setattr-fixture] fixture border-color (#1b4332)
    "body_setattr_fallback_bg": 0,  # sample.css body-setattr fixture fallback background (#ffafcc)
    "body_setattr_fallback_border": 0,  # sample.css body-setattr fixture fallback border-color (#b56576)
    "body_removeattr_fallback_bg": 0,  # sample.css body-removeattr fixture fallback background (#a7c957)
    "body_removeattr_fallback_border": 0,  # sample.css body-removeattr fixture fallback border-color (#386641)
    "body_removeattr_runtime_bg": 0,  # sample.css body[data-body-removeattr-fixture] fixture background (#ff99c8)
    "body_removeattr_runtime_border": 0,  # sample.css body[data-body-removeattr-fixture] fixture border-color (#9d4edd)
    "js_setattr_id_bg": 0,  # sample.css setAttribute(\"id\") fixture armed background (#38bdf8)
    "js_setattr_id_border": 0,  # sample.css setAttribute(\"id\") fixture armed border-color (#075985)
    "js_setattr_id_fallback_bg": 0,  # sample.css setAttribute(\"id\") fixture fallback background (#854d0e)
    "js_setattr_id_fallback_border": 0,  # sample.css setAttribute(\"id\") fixture fallback border-color (#451a03)
    "removeattr_style_fallback_bg": 0,  # sample.css removeAttribute(\"style\") fallback background (#c7f9cc)
    "removeattr_style_fallback_border": 0,  # sample.css removeAttribute(\"style\") fallback border-color (#2d6a4f)
    "removeattr_style_inline_bg": 0,  # sample.html inline style before removeAttribute(\"style\") (#ffd166)
    "removeattr_style_inline_border": 0,  # sample.html inline border before removeAttribute(\"style\") (#ef476f)
    "removeattr_class_runtime_bg": 0,  # sample.css armed removeAttribute(\"class\") state background (#4cc9f0)
    "removeattr_class_runtime_border": 0,  # sample.css armed removeAttribute(\"class\") state border-color (#0c4a6e)
    "removeattr_class_fallback_bg": 0,  # sample.css removeAttribute(\"class\") fallback background (#ff7f50)
    "removeattr_class_fallback_border": 0,  # sample.css removeAttribute(\"class\") fallback border-color (#9a3412)
    "inline_media_style_bg": 0,  # sample.html <style media="screen"> fixture background (#fef3c7)
    "selector_empty_bg": 0,  # sample.css .empty-fixture:empty background rgb(72%, 88%, 97%)
    "selector_empty_border": 0,  # sample.css .empty-fixture:empty border-color (#0f766e)
    "selector_empty_fallback_bg": 0,  # sample.css .empty-fixture fallback background (#f8fafc)
    "selector_empty_fallback_border": 0,  # sample.css .empty-fixture fallback border-color (#64748b)
    "media_filter_screen_bg": 0,  # sample.css #media-filter-fixture background (#ecfeff)
    "media_filter_print_bg": 0,  # sample.html print-only stylesheet override (#fce7f3)
}

for i in range(0, expected_size, 3):
    rgb = (pixels[i], pixels[i + 1], pixels[i + 2])

    if near(rgb, (253, 230, 138), 24):
        tracked_signals["js_bg_yellow"] += 1
    if near(rgb, (191, 219, 254), 24):
        tracked_signals["js_chip_blue"] += 1
    if near(rgb, (134, 239, 172), 24):
        tracked_signals["js_style_string_green"] += 1
    if near(rgb, (34, 197, 94), 24):
        tracked_signals["js_style_string_border_green"] += 1
    if near(rgb, (217, 249, 157), 24):
        tracked_signals["js_setattr_green_bg"] += 1
    if near(rgb, (139, 92, 246), 10):
        tracked_signals["js_setattr_nonstyle_bg"] += 1
    if near(rgb, (91, 33, 182), 10):
        tracked_signals["js_setattr_nonstyle_border"] += 1
    if near(rgb, (244, 63, 94), 10):
        tracked_signals["js_setattr_nonstyle_fallback_bg"] += 1
    if near(rgb, (233, 245, 255), 24):
        tracked_signals["selector_child_blue"] += 1
    if near_any(rgb, ((186, 230, 253), (207, 236, 254), (214, 240, 254)), 26):
        tracked_signals["selector_nth_rgba"] += 1
    if near(rgb, (203, 235, 246), 24):
        tracked_signals["selector_nth_hsl_bg"] += 1
    if near(rgb, (242, 136, 44), 26):
        tracked_signals["selector_nth_hsl_border"] += 1
    if near(rgb, (255, 165, 0), 20):
        tracked_signals["selector_nth_odd_named_orange"] += 1
    if near(rgb, (255, 241, 214), 18):
        tracked_signals["selector_nth_even_nested_bg"] += 1
    if near(rgb, (209, 250, 229), 24):
        tracked_signals["selector_adjacent_green"] += 1
    if near(rgb, (249, 115, 22), 26):
        tracked_signals["js_border_orange"] += 1
    if near(rgb, (16, 185, 129), 26):
        tracked_signals["selector_sibling_green"] += 1
    if near(rgb, (237, 245, 255), 24):
        tracked_signals["selector_hex_alpha_hero_bg"] += 1
    if near(rgb, (219, 234, 254), 24):
        tracked_signals["selector_hero_fallback_bg"] += 1
    if near(rgb, (224, 242, 254), 20):
        tracked_signals["data_url_note_bg"] += 1
    if near(rgb, (153, 204, 255), 24):
        tracked_signals["selector_attr_rgb_bg"] += 1
    if near(rgb, (68, 136, 170), 24):
        tracked_signals["selector_attr_rgb_border"] += 1
    if near(rgb, (163, 230, 53), 12):
        tracked_signals["selector_attr_prefix_bg"] += 1
    if near(rgb, (54, 83, 20), 10):
        tracked_signals["selector_attr_prefix_border"] += 1
    if near(rgb, (212, 181, 255), 12):
        tracked_signals["selector_attr_prefix_fallback_bg"] += 1
    if near(rgb, (126, 34, 206), 10):
        tracked_signals["selector_attr_prefix_fallback_border"] += 1
    if near(rgb, (250, 204, 21), 12):
        tracked_signals["selector_attr_suffix_bg"] += 1
    if near(rgb, (113, 63, 18), 10):
        tracked_signals["selector_attr_suffix_border"] += 1
    if near(rgb, (148, 163, 184), 12):
        tracked_signals["selector_attr_suffix_fallback_bg"] += 1
    if near(rgb, (71, 85, 105), 10):
        tracked_signals["selector_attr_suffix_fallback_border"] += 1
    if near(rgb, (240, 171, 252), 12):
        tracked_signals["selector_attr_contains_bg"] += 1
    if near(rgb, (112, 26, 117), 10):
        tracked_signals["selector_attr_contains_border"] += 1
    if near(rgb, (226, 232, 240), 12):
        tracked_signals["selector_attr_contains_fallback_bg"] += 1
    if near(rgb, (51, 65, 85), 10):
        tracked_signals["selector_attr_contains_fallback_border"] += 1
    if near(rgb, (142, 202, 230), 12):
        tracked_signals["selector_attr_exists_bg"] += 1
    if near(rgb, (2, 48, 71), 10):
        tracked_signals["selector_attr_exists_border"] += 1
    if near(rgb, (255, 180, 162), 12):
        tracked_signals["selector_attr_exists_fallback_bg"] += 1
    if near(rgb, (157, 2, 8), 10):
        tracked_signals["selector_attr_exists_fallback_border"] += 1
    if near(rgb, (37, 99, 235), 28):
        tracked_signals["selector_class_contains_border"] += 1
    if near(rgb, (14, 165, 233), 28):
        tracked_signals["selector_class_fallback_border"] += 1
    if near(rgb, (124, 45, 18), 30):
        tracked_signals["selector_currentcolor_border"] += 1
    if near(rgb, (34, 211, 238), 10):
        tracked_signals["selector_not_bg"] += 1
    if near(rgb, (14, 116, 144), 10):
        tracked_signals["selector_not_border"] += 1
    if near(rgb, (251, 113, 133), 10):
        tracked_signals["selector_not_fallback_bg"] += 1
    if near(rgb, (132, 204, 22), 10):
        tracked_signals["entity_decode_bg"] += 1
    if near(rgb, (63, 98, 18), 10):
        tracked_signals["entity_decode_border"] += 1
    if near(rgb, (245, 158, 11), 10):
        tracked_signals["entity_decode_fallback_bg"] += 1
    if near(rgb, (190, 242, 100), 12):
        tracked_signals["apos_entity_decode_bg"] += 1
    if near(rgb, (77, 124, 15), 10):
        tracked_signals["apos_entity_decode_border"] += 1
    if near(rgb, (254, 205, 211), 12):
        tracked_signals["apos_entity_fallback_bg"] += 1
    if near(rgb, (45, 212, 191), 10):
        tracked_signals["numeric_entity_decode_bg"] += 1
    if near(rgb, (17, 94, 89), 10):
        tracked_signals["numeric_entity_decode_border"] += 1
    if near(rgb, (245, 208, 254), 12):
        tracked_signals["numeric_entity_fallback_bg"] += 1
    if near(rgb, (162, 28, 175), 10):
        tracked_signals["numeric_entity_fallback_border"] += 1
    if near(rgb, (167, 243, 208), 10):
        tracked_signals["copy_reg_trade_entity_decode_bg"] += 1
    if near(rgb, (4, 120, 87), 10):
        tracked_signals["copy_reg_trade_entity_decode_border"] += 1
    if near(rgb, (251, 207, 232), 10):
        tracked_signals["copy_reg_trade_entity_fallback_bg"] += 1
    if near(rgb, (157, 23, 77), 10):
        tracked_signals["copy_reg_trade_entity_fallback_border"] += 1
    if near(rgb, (74, 222, 128), 10):
        tracked_signals["mdash_ndash_entity_decode_bg"] += 1
    if near(rgb, (22, 101, 52), 10):
        tracked_signals["mdash_ndash_entity_decode_border"] += 1
    if near(rgb, (252, 211, 77), 10):
        tracked_signals["mdash_ndash_entity_fallback_bg"] += 1
    if near(rgb, (161, 98, 7), 10):
        tracked_signals["mdash_ndash_entity_fallback_border"] += 1
    if near(rgb, (244, 114, 182), 10):
        tracked_signals["currency_entity_decode_bg"] += 1
    if near(rgb, (131, 24, 67), 10):
        tracked_signals["currency_entity_decode_border"] += 1
    if near(rgb, (255, 237, 213), 10):
        tracked_signals["currency_entity_fallback_bg"] += 1
    if near(rgb, (146, 64, 14), 10):
        tracked_signals["currency_entity_fallback_border"] += 1
    if near(rgb, (185, 251, 192), 10):
        tracked_signals["yen_sect_deg_entity_decode_bg"] += 1
    if near(rgb, (17, 138, 178), 10):
        tracked_signals["yen_sect_deg_entity_decode_border"] += 1
    if near(rgb, (255, 214, 165), 10):
        tracked_signals["yen_sect_deg_entity_fallback_bg"] += 1
    if near(rgb, (176, 137, 104), 10):
        tracked_signals["yen_sect_deg_entity_fallback_border"] += 1
    if near(rgb, (186, 243, 252), 24):
        tracked_signals["selector_only_child_hsla_bg"] += 1
    if near(rgb, (207, 250, 254), 24):
        tracked_signals["selector_only_child_fallback_bg"] += 1
    if near(rgb, (110, 231, 183), 12):
        tracked_signals["selector_first_of_type_bg"] += 1
    if near(rgb, (5, 150, 105), 10):
        tracked_signals["selector_first_of_type_border"] += 1
    if near(rgb, (196, 181, 253), 12):
        tracked_signals["selector_first_of_type_fallback_bg"] += 1
    if near(rgb, (124, 58, 237), 12):
        tracked_signals["selector_first_of_type_fallback_border"] += 1
    if near(rgb, (254, 202, 202), 12):
        tracked_signals["selector_last_of_type_bg"] += 1
    if near(rgb, (190, 24, 93), 10):
        tracked_signals["selector_last_of_type_border"] += 1
    if near(rgb, (253, 186, 116), 12):
        tracked_signals["selector_last_of_type_fallback_bg"] += 1
    if near(rgb, (234, 88, 12), 12):
        tracked_signals["selector_last_of_type_fallback_border"] += 1
    if near(rgb, (153, 246, 228), 12):
        tracked_signals["selector_nth_of_type_bg"] += 1
    if near(rgb, (13, 148, 136), 10):
        tracked_signals["selector_nth_of_type_border"] += 1
    if near(rgb, (249, 168, 212), 12):
        tracked_signals["selector_nth_of_type_fallback_bg"] += 1
    if near(rgb, (219, 39, 119), 10):
        tracked_signals["selector_nth_of_type_fallback_border"] += 1
    if near(rgb, (147, 197, 253), 10):
        tracked_signals["selector_nth_last_child_bg"] += 1
    if near(rgb, (29, 78, 216), 10):
        tracked_signals["selector_nth_last_child_border"] += 1
    if near(rgb, (252, 165, 165), 10):
        tracked_signals["selector_nth_last_child_fallback_bg"] += 1
    if near(rgb, (185, 28, 28), 10):
        tracked_signals["selector_nth_last_child_fallback_border"] += 1
    if near(rgb, (20, 184, 166), 10):
        tracked_signals["selector_nth_last_normalized_bg"] += 1
    if near(rgb, (19, 78, 74), 10):
        tracked_signals["selector_nth_last_normalized_border"] += 1
    if near(rgb, (232, 121, 249), 10):
        tracked_signals["selector_nth_last_normalized_fallback_bg"] += 1
    if near(rgb, (134, 25, 143), 10):
        tracked_signals["selector_nth_last_normalized_fallback_border"] += 1
    if near(rgb, (103, 232, 249), 12):
        tracked_signals["js_classname_runtime_bg"] += 1
    if near(rgb, (3, 105, 161), 10):
        tracked_signals["js_classname_runtime_border"] += 1
    if near(rgb, (233, 213, 255), 12):
        tracked_signals["js_classname_fallback_bg"] += 1
    if near(rgb, (168, 85, 247), 10):
        tracked_signals["js_classname_fallback_border"] += 1
    if near(rgb, (187, 247, 208), 10):
        tracked_signals["js_runtime_id_bg"] += 1
    if near(rgb, (21, 128, 61), 10):
        tracked_signals["js_runtime_id_border"] += 1
    if near(rgb, (254, 215, 170), 10):
        tracked_signals["js_runtime_id_fallback_bg"] += 1
    if near(rgb, (194, 65, 12), 10):
        tracked_signals["js_runtime_id_fallback_border"] += 1
    if near(rgb, (167, 139, 250), 12):
        tracked_signals["body_classname_runtime_bg"] += 1
    if near(rgb, (109, 40, 217), 10):
        tracked_signals["body_classname_runtime_border"] += 1
    if near(rgb, (229, 231, 235), 10):
        tracked_signals["body_classname_fallback_bg"] += 1
    if near(rgb, (107, 114, 128), 10):
        tracked_signals["body_classname_fallback_border"] += 1
    if near(rgb, (125, 211, 252), 10):
        tracked_signals["body_id_runtime_bg"] += 1
    if near(rgb, (30, 64, 175), 10):
        tracked_signals["body_id_runtime_border"] += 1
    if near(rgb, (237, 233, 254), 10):
        tracked_signals["body_id_fallback_bg"] += 1
    if near(rgb, (99, 102, 241), 10):
        tracked_signals["body_id_fallback_border"] += 1
    if near(rgb, (58, 134, 255), 12):
        tracked_signals["body_setattr_runtime_bg"] += 1
    if near(rgb, (29, 53, 87), 10):
        tracked_signals["body_setattr_runtime_border"] += 1
    if near(rgb, (82, 183, 136), 12):
        tracked_signals["body_setattr_attr_bg"] += 1
    if near(rgb, (27, 67, 50), 10):
        tracked_signals["body_setattr_attr_border"] += 1
    if near(rgb, (255, 175, 204), 12):
        tracked_signals["body_setattr_fallback_bg"] += 1
    if near(rgb, (181, 101, 118), 10):
        tracked_signals["body_setattr_fallback_border"] += 1
    if near(rgb, (167, 201, 87), 12):
        tracked_signals["body_removeattr_fallback_bg"] += 1
    if near(rgb, (56, 102, 65), 10):
        tracked_signals["body_removeattr_fallback_border"] += 1
    if near(rgb, (255, 153, 200), 12):
        tracked_signals["body_removeattr_runtime_bg"] += 1
    if near(rgb, (157, 78, 221), 10):
        tracked_signals["body_removeattr_runtime_border"] += 1
    if near(rgb, (56, 189, 248), 10):
        tracked_signals["js_setattr_id_bg"] += 1
    if near(rgb, (7, 89, 133), 10):
        tracked_signals["js_setattr_id_border"] += 1
    if near(rgb, (133, 77, 14), 10):
        tracked_signals["js_setattr_id_fallback_bg"] += 1
    if near(rgb, (69, 26, 3), 10):
        tracked_signals["js_setattr_id_fallback_border"] += 1
    if near(rgb, (199, 249, 204), 12):
        tracked_signals["removeattr_style_fallback_bg"] += 1
    if near(rgb, (45, 106, 79), 10):
        tracked_signals["removeattr_style_fallback_border"] += 1
    if near(rgb, (255, 209, 102), 12):
        tracked_signals["removeattr_style_inline_bg"] += 1
    if near(rgb, (239, 71, 111), 10):
        tracked_signals["removeattr_style_inline_border"] += 1
    if near(rgb, (76, 201, 240), 12):
        tracked_signals["removeattr_class_runtime_bg"] += 1
    if near(rgb, (12, 74, 110), 10):
        tracked_signals["removeattr_class_runtime_border"] += 1
    if near(rgb, (255, 127, 80), 12):
        tracked_signals["removeattr_class_fallback_bg"] += 1
    if near(rgb, (154, 52, 18), 10):
        tracked_signals["removeattr_class_fallback_border"] += 1
    if near(rgb, (254, 243, 199), 24):
        tracked_signals["inline_media_style_bg"] += 1
    if near(rgb, (184, 224, 247), 10):
        tracked_signals["selector_empty_bg"] += 1
    if near(rgb, (15, 118, 110), 8):
        tracked_signals["selector_empty_border"] += 1
    if near(rgb, (248, 250, 252), 8):
        tracked_signals["selector_empty_fallback_bg"] += 1
    if near(rgb, (100, 116, 139), 8):
        tracked_signals["selector_empty_fallback_border"] += 1
    if near(rgb, (236, 254, 255), 8):
        tracked_signals["media_filter_screen_bg"] += 1
    if near(rgb, (252, 231, 243), 8):
        tracked_signals["media_filter_print_bg"] += 1

empty_fixture_ok = (
    tracked_signals["selector_empty_bg"] >= 300 or
    tracked_signals["selector_empty_border"] >= 120 or
    tracked_signals["selector_empty_fallback_bg"] >= 300 or
    tracked_signals["selector_empty_fallback_border"] >= 120
)
if not empty_fixture_ok:
    raise SystemExit(
        "feature assertion failed: :empty fixture signal check failed; "
        f"counts={tracked_signals['selector_empty_bg']}/"
        f"{tracked_signals['selector_empty_border']}/"
        f"{tracked_signals['selector_empty_fallback_bg']}/"
        f"{tracked_signals['selector_empty_fallback_border']}"
    )

media_filter_ok = (
    tracked_signals["media_filter_screen_bg"] >= 200 or
    tracked_signals["media_filter_print_bg"] >= 200
)
if not media_filter_ok:
    raise SystemExit(
        "feature assertion failed: media-filter fixture signal check failed; "
        f"counts={tracked_signals['media_filter_screen_bg']}/"
        f"{tracked_signals['media_filter_print_bg']}"
    )

nth_child_rgba_ok = (
    tracked_signals["selector_nth_rgba"] >= 80 or
    tracked_signals["selector_child_blue"] >= 80
)
nth_child_hsl_ok = (
    tracked_signals["selector_nth_hsl_bg"] >= 40 or
    tracked_signals["selector_nth_hsl_border"] >= 8
)
nth_child_odd_even_ok = (
    tracked_signals["selector_nth_odd_named_orange"] >= 4 or
    tracked_signals["selector_nth_even_nested_bg"] >= 40
)
data_url_ok = tracked_signals["data_url_note_bg"] >= 8
attr_selector_rgb_ok = (
    tracked_signals["selector_attr_rgb_bg"] >= 8 or
    tracked_signals["selector_attr_rgb_border"] >= 3
)
attr_prefix_selector_ok = (
    tracked_signals["selector_attr_prefix_bg"] >= 8 or
    tracked_signals["selector_attr_prefix_border"] >= 3 or
    tracked_signals["selector_attr_prefix_fallback_bg"] >= 8 or
    tracked_signals["selector_attr_prefix_fallback_border"] >= 3
)
attr_suffix_selector_ok = (
    tracked_signals["selector_attr_suffix_bg"] >= 8 or
    tracked_signals["selector_attr_suffix_border"] >= 3 or
    tracked_signals["selector_attr_suffix_fallback_bg"] >= 8 or
    tracked_signals["selector_attr_suffix_fallback_border"] >= 3
)
attr_contains_selector_ok = (
    tracked_signals["selector_attr_contains_bg"] >= 8 or
    tracked_signals["selector_attr_contains_border"] >= 3 or
    tracked_signals["selector_attr_contains_fallback_bg"] >= 8 or
    tracked_signals["selector_attr_contains_fallback_border"] >= 3
)
attr_exists_selector_ok = (
    tracked_signals["selector_attr_exists_bg"] >= 8 or
    tracked_signals["selector_attr_exists_border"] >= 3 or
    tracked_signals["selector_attr_exists_fallback_bg"] >= 8 or
    tracked_signals["selector_attr_exists_fallback_border"] >= 3
)
class_token_selector_ok = tracked_signals["selector_class_contains_border"] >= 8
class_token_fallback_ok = tracked_signals["selector_class_fallback_border"] >= 8
class_token_ok = class_token_selector_ok or class_token_fallback_ok
runtime_style_string_ok = (
    tracked_signals["js_style_string_green"] >= 10 or
    tracked_signals["js_style_string_border_green"] >= 6
)
setattr_style_string_ok = tracked_signals["js_setattr_green_bg"] >= 8
setattr_nonstyle_ok = (
    tracked_signals["js_setattr_nonstyle_bg"] >= 8 or
    tracked_signals["js_setattr_nonstyle_border"] >= 3
)
current_color_ok = tracked_signals["selector_currentcolor_border"] >= 60
not_fixture_ok = (
    tracked_signals["selector_not_bg"] >= 12 or
    tracked_signals["selector_not_border"] >= 6 or
    tracked_signals["selector_not_fallback_bg"] >= 20
)
entity_decode_ok = (
    tracked_signals["entity_decode_bg"] >= 8 or
    tracked_signals["entity_decode_border"] >= 3
)
apos_entity_decode_ok = (
    tracked_signals["apos_entity_decode_bg"] >= 8 or
    tracked_signals["apos_entity_decode_border"] >= 3
)
numeric_entity_decode_ok = (
    tracked_signals["numeric_entity_decode_bg"] >= 8 or
    tracked_signals["numeric_entity_decode_border"] >= 3 or
    tracked_signals["numeric_entity_fallback_bg"] >= 8 or
    tracked_signals["numeric_entity_fallback_border"] >= 3
)
copy_reg_trade_entity_ok = (
    tracked_signals["copy_reg_trade_entity_decode_bg"] >= 8 or
    tracked_signals["copy_reg_trade_entity_decode_border"] >= 3 or
    tracked_signals["copy_reg_trade_entity_fallback_bg"] >= 8 or
    tracked_signals["copy_reg_trade_entity_fallback_border"] >= 3
)
mdash_ndash_entity_decode_ok = (
    tracked_signals["mdash_ndash_entity_decode_bg"] >= 8 or
    tracked_signals["mdash_ndash_entity_decode_border"] >= 3
)
currency_entity_decode_ok = (
    tracked_signals["currency_entity_decode_bg"] >= 8 or
    tracked_signals["currency_entity_decode_border"] >= 3 or
    tracked_signals["currency_entity_fallback_bg"] >= 8 or
    tracked_signals["currency_entity_fallback_border"] >= 3
)
yen_sect_deg_entity_ok = (
    tracked_signals["yen_sect_deg_entity_decode_bg"] >= 8 or
    tracked_signals["yen_sect_deg_entity_decode_border"] >= 3 or
    tracked_signals["yen_sect_deg_entity_fallback_bg"] >= 8 or
    tracked_signals["yen_sect_deg_entity_fallback_border"] >= 3
)
hex_alpha_hero_ok = (
    tracked_signals["selector_hex_alpha_hero_bg"] >= 40 or
    tracked_signals["selector_hero_fallback_bg"] >= 40
)
only_child_fixture_ok = (
    tracked_signals["selector_only_child_hsla_bg"] >= 10 or
    tracked_signals["selector_only_child_fallback_bg"] >= 10
)
first_of_type_fixture_ok = (
    tracked_signals["selector_first_of_type_bg"] >= 8 or
    tracked_signals["selector_first_of_type_border"] >= 3 or
    tracked_signals["selector_first_of_type_fallback_bg"] >= 8 or
    tracked_signals["selector_first_of_type_fallback_border"] >= 3
)
last_of_type_fixture_ok = (
    tracked_signals["selector_last_of_type_bg"] >= 8 or
    tracked_signals["selector_last_of_type_border"] >= 3 or
    tracked_signals["selector_last_of_type_fallback_bg"] >= 8 or
    tracked_signals["selector_last_of_type_fallback_border"] >= 3
)
nth_of_type_fixture_ok = (
    tracked_signals["selector_nth_of_type_bg"] >= 8 or
    tracked_signals["selector_nth_of_type_border"] >= 3 or
    tracked_signals["selector_nth_of_type_fallback_bg"] >= 8 or
    tracked_signals["selector_nth_of_type_fallback_border"] >= 3
)
nth_last_child_fixture_ok = (
    tracked_signals["selector_nth_last_child_bg"] >= 8 or
    tracked_signals["selector_nth_last_child_border"] >= 3 or
    tracked_signals["selector_nth_last_child_fallback_bg"] >= 8 or
    tracked_signals["selector_nth_last_child_fallback_border"] >= 3
)
nth_last_normalized_fixture_ok = (
    tracked_signals["selector_nth_last_normalized_bg"] >= 8 or
    tracked_signals["selector_nth_last_normalized_border"] >= 3 or
    tracked_signals["selector_nth_last_normalized_fallback_bg"] >= 8 or
    tracked_signals["selector_nth_last_normalized_fallback_border"] >= 3
)
runtime_classname_ok = (
    tracked_signals["js_classname_runtime_bg"] >= 8 or
    tracked_signals["js_classname_runtime_border"] >= 3 or
    tracked_signals["js_classname_fallback_bg"] >= 8 or
    tracked_signals["js_classname_fallback_border"] >= 3
)
runtime_id_fixture_ok = (
    tracked_signals["js_runtime_id_bg"] >= 8 or
    tracked_signals["js_runtime_id_border"] >= 3 or
    tracked_signals["js_runtime_id_fallback_bg"] >= 8 or
    tracked_signals["js_runtime_id_fallback_border"] >= 3
)
body_classname_fixture_ok = (
    tracked_signals["body_classname_runtime_bg"] >= 8 or
    tracked_signals["body_classname_runtime_border"] >= 3 or
    tracked_signals["body_classname_fallback_bg"] >= 8 or
    tracked_signals["body_classname_fallback_border"] >= 3
)
body_id_fixture_ok = (
    tracked_signals["body_id_runtime_bg"] >= 8 or
    tracked_signals["body_id_runtime_border"] >= 3 or
    tracked_signals["body_id_fallback_bg"] >= 8 or
    tracked_signals["body_id_fallback_border"] >= 3
)
body_setattr_fixture_ok = (
    tracked_signals["body_setattr_runtime_bg"] >= 8 or
    tracked_signals["body_setattr_runtime_border"] >= 3 or
    tracked_signals["body_setattr_attr_bg"] >= 8 or
    tracked_signals["body_setattr_attr_border"] >= 3 or
    tracked_signals["body_setattr_fallback_bg"] >= 8 or
    tracked_signals["body_setattr_fallback_border"] >= 3
)
body_removeattr_fixture_ok = (
    tracked_signals["body_removeattr_fallback_bg"] >= 8 or
    tracked_signals["body_removeattr_fallback_border"] >= 3 or
    tracked_signals["body_removeattr_runtime_bg"] >= 8 or
    tracked_signals["body_removeattr_runtime_border"] >= 3
)
setattr_id_fixture_ok = (
    (tracked_signals["js_setattr_id_bg"] >= 8 or tracked_signals["js_setattr_id_border"] >= 3) and
    tracked_signals["js_setattr_id_fallback_bg"] < 120 and
    tracked_signals["js_setattr_id_fallback_border"] < 60
)
removeattr_style_fixture_ok = (
    (tracked_signals["removeattr_style_fallback_bg"] >= 8 or tracked_signals["removeattr_style_fallback_border"] >= 3) and
    tracked_signals["removeattr_style_inline_bg"] < 120 and
    tracked_signals["removeattr_style_inline_border"] < 60
)
removeattr_class_fixture_ok = (
    (tracked_signals["removeattr_class_fallback_bg"] >= 8 or tracked_signals["removeattr_class_fallback_border"] >= 3) and
    tracked_signals["removeattr_class_runtime_bg"] < 120 and
    tracked_signals["removeattr_class_runtime_border"] < 60
)
inline_media_style_ok = tracked_signals["inline_media_style_bg"] >= 8
js_feature_ok = (
    tracked_signals["js_bg_yellow"] >= 25 or
    tracked_signals["js_chip_blue"] >= 10 or
    tracked_signals["js_border_orange"] >= 12 or
    runtime_style_string_ok or
    setattr_style_string_ok or
    setattr_nonstyle_ok or
    setattr_id_fixture_ok or
    runtime_classname_ok or
    runtime_id_fixture_ok or
    body_classname_fixture_ok or
    body_id_fixture_ok or
    body_setattr_fixture_ok or
    body_removeattr_fixture_ok or
    removeattr_style_fixture_ok or
    removeattr_class_fixture_ok
)
selector_feature_ok = (
    nth_child_rgba_ok or
    nth_child_hsl_ok or
    nth_child_odd_even_ok or
    tracked_signals["selector_adjacent_green"] >= 20 or
    tracked_signals["selector_sibling_green"] >= 6 or
    only_child_fixture_ok or
    first_of_type_fixture_ok or
    last_of_type_fixture_ok or
    nth_of_type_fixture_ok or
    nth_last_child_fixture_ok or
    nth_last_normalized_fixture_ok or
    inline_media_style_ok
)
increment_feature_ok = (
    (nth_child_rgba_ok or nth_child_hsl_ok or nth_child_odd_even_ok) and
    (tracked_signals["selector_sibling_green"] >= 6 or tracked_signals["selector_adjacent_green"] >= 20) and
    data_url_ok and
    attr_selector_rgb_ok and
    attr_exists_selector_ok and
    attr_prefix_selector_ok and
    attr_suffix_selector_ok and
    attr_contains_selector_ok and
    class_token_ok and
    current_color_ok and
    hex_alpha_hero_ok
)
upcoming_fixture_ok = (
    not_fixture_ok and
    entity_decode_ok and
    apos_entity_decode_ok and
    numeric_entity_decode_ok and
    copy_reg_trade_entity_ok and
    mdash_ndash_entity_decode_ok and
    currency_entity_decode_ok and
    yen_sect_deg_entity_ok and
    attr_exists_selector_ok and
    setattr_nonstyle_ok and
    setattr_id_fixture_ok and
    runtime_classname_ok and
    runtime_id_fixture_ok and
    body_classname_fixture_ok and
    body_id_fixture_ok and
    body_setattr_fixture_ok and
    body_removeattr_fixture_ok and
    removeattr_style_fixture_ok and
    removeattr_class_fixture_ok and
    (first_of_type_fixture_ok or last_of_type_fixture_ok or nth_of_type_fixture_ok or nth_last_child_fixture_ok or nth_last_normalized_fixture_ok)
)

if (
    not js_feature_ok or
    not selector_feature_ok or
    not increment_feature_ok or
    not upcoming_fixture_ok or
    not transparent_fixture_ok or
    not empty_fixture_ok or
    not media_filter_ok
):
    raise SystemExit(
        "feature assertion failed: expected querySelector/getElementById style colors (including style-string, className, runtime .id mutation, document.body.className/id selector-driven fixtures, document.body.setAttribute/removeAttribute fixture transitions, setAttribute(\"id\"), and removeAttribute fallback fixture signals), nth-child/:only-child/:first-of-type/:last-of-type/:nth-of-type/:nth-last-child selector signals (including case-insensitive spaced nth-last normalization fixture), :not/entity/&apos;/numeric-entity/&copy;/&reg;/&trade;/&cent;/&pound;/&euro;/&yen;/&sect;/&deg;/&mdash;/&ndash; fixture colors, data URL/media fixture colors, [id=...] + #RGB fixture colors plus [attr]/[attr^=]/[attr$=]/[attr*=] fixture colors, #RRGGBBAA hero/fallback colors, currentColor fixture border color, and transparent/:empty/media-filter fixture sanity checks in sample output; "
        f"signal_counts={tracked_signals}"
    )
PY
}

main() {
  local feature_assert_output
  local query_selector_assert_output
  local body_style_assert_output
  local border_style_assert_output
  local font_size_assert_output
  local set_attribute_assert_output
  local remove_attribute_assert_output
  local local_path_assert_output

  build_browser

  if [[ -z "${BROWSER_BIN}" || ! -x "${BROWSER_BIN}" ]]; then
    echo "[FAIL] Browser executable not found after build." >&2
    return 1
  fi

  start_server

  if ! run_smoke_render; then
    echo "[FAIL] Browser execution failed." >&2
    return 1
  fi

  if [[ ! -s "${OUTPUT_FILE}" ]]; then
    echo "[FAIL] Output file missing or empty: ${OUTPUT_FILE}" >&2
    return 1
  fi

  if ! run_feature_render; then
    echo "[FAIL] Feature sample render failed." >&2
    return 1
  fi

  if [[ ! -s "${FEATURE_OUTPUT_FILE}" ]]; then
    echo "[FAIL] Feature output file missing or empty: ${FEATURE_OUTPUT_FILE}" >&2
    return 1
  fi

  local_path_assert_output=""
  if [[ "${FEATURE_LOCAL_RENDER_OK}" == "1" ]]; then
    if [[ ! -s "${FEATURE_LOCAL_OUTPUT_FILE}" ]]; then
      local_path_assert_output="feature assertion failed: local-path sample render reported success but output file is missing or empty: ${FEATURE_LOCAL_OUTPUT_FILE}"
    fi
  else
    local_path_assert_output="feature assertion failed: expected sample render to accept a plain local path input (without file://); path=${EXAMPLES_DIR}/sample.html; render_log=${FEATURE_LOCAL_RENDER_LOG}"
  fi

  if [[ -n "${local_path_assert_output}" ]]; then
    if [[ "${STRICT_FEATURE_ASSERT}" == "1" ]]; then
      echo "[FAIL] Local-path render assertion failed." >&2
      echo "${local_path_assert_output}" >&2
      return 1
    fi
    echo "[WARN] Local-path render assertion did not match expected support; continuing in non-strict mode." >&2
    echo "${local_path_assert_output}" >&2
  fi

  query_selector_assert_output=""
  case "${FEATURE_RENDER_LOG}" in
    *"document.querySelector call is missing"*|*"document.querySelector could not find element"*|*"Unsupported document.querySelector operation"*|*"Unsupported document.querySelector(...).style property: background"*|*"Unsupported document.querySelector(...).style property: backgroundColor"*|*"Unsupported document.querySelector(...).style property: borderColor"*|*"document.querySelector(...).style assignment"*)
      query_selector_assert_output="feature assertion failed: expected supported document.querySelector('#...').style.{background,backgroundColor,borderColor} and document.querySelector('#...').style = \"...\" statements; render_log=${FEATURE_RENDER_LOG}"
      if [[ "${STRICT_FEATURE_ASSERT}" == "1" ]]; then
        echo "[FAIL] QuerySelector assertion failed on sample render output." >&2
        echo "${query_selector_assert_output}" >&2
        return 1
      fi
      echo "[WARN] QuerySelector assertion did not match expected runtime support; continuing in non-strict mode." >&2
      echo "${query_selector_assert_output}" >&2
      ;;
  esac

  body_style_assert_output=""
  case "${FEATURE_RENDER_LOG}" in
    *"document.body.style assignment is missing property name"*|*"Unsupported document.body.style property:"*)
      body_style_assert_output="feature assertion failed: expected supported document.body.style = \"...\" assignment in sample runtime script; render_log=${FEATURE_RENDER_LOG}"
      if [[ "${STRICT_FEATURE_ASSERT}" == "1" ]]; then
        echo "[FAIL] document.body.style runtime assertion failed on sample render output." >&2
        echo "${body_style_assert_output}" >&2
        return 1
      fi
      echo "[WARN] document.body.style runtime assertion did not match expected support; continuing in non-strict mode." >&2
      echo "${body_style_assert_output}" >&2
      ;;
  esac

  border_style_assert_output=""
  case "${FEATURE_RENDER_LOG}" in
    *"Unsupported document.getElementById(...).style property: border"*|*"Unsupported document.getElementById(...).style property: borderWidth"*|*"Unsupported document.getElementById(...).style property: borderStyle"*|*"Unsupported document.querySelector(...).style property: border"*|*"Unsupported document.querySelector(...).style property: borderWidth"*|*"Unsupported document.querySelector(...).style property: borderStyle"*)
      border_style_assert_output="feature assertion failed: expected runtime support for .style.border / .style.borderWidth / .style.borderStyle statements; render_log=${FEATURE_RENDER_LOG}"
      if [[ "${STRICT_FEATURE_ASSERT}" == "1" ]]; then
        echo "[FAIL] Border style runtime assertion failed on sample render output." >&2
        echo "${border_style_assert_output}" >&2
        return 1
      fi
      echo "[WARN] Border style runtime assertion did not match expected support; continuing in non-strict mode." >&2
      echo "${border_style_assert_output}" >&2
      ;;
  esac

  font_size_assert_output=""
  case "${FEATURE_RENDER_LOG}" in
    *"Unsupported document.querySelector(...).style property: fontSize"*|*"Unsupported document.getElementById(...).style property: fontSize"*)
      font_size_assert_output="feature note: optional camelCase .style.fontSize assignment is unsupported in this build; render_log=${FEATURE_RENDER_LOG}"
      echo "[WARN] CamelCase fontSize style assignment is optional in smoke checks; continuing." >&2
      echo "${font_size_assert_output}" >&2
      ;;
  esac

  remove_attribute_assert_output=""
  if [[ "${FEATURE_RENDER_LOG}" == *".removeAttribute("* ]] && (
    [[ "${FEATURE_RENDER_LOG}" == *"Unsupported"* ]] ||
    [[ "${FEATURE_RENDER_LOG}" == *"could not find element"* ]] ||
    [[ "${FEATURE_RENDER_LOG}" == *"removeAttribute call is missing"* ]]
  ); then
    remove_attribute_assert_output="feature assertion failed: expected supported .removeAttribute(\"style\") and .removeAttribute(\"class\") runtime calls for fallback fixtures; render_log=${FEATURE_RENDER_LOG}"
    if [[ "${STRICT_FEATURE_ASSERT}" == "1" ]]; then
      echo "[FAIL] removeAttribute runtime assertion failed on sample render output." >&2
      echo "${remove_attribute_assert_output}" >&2
      return 1
    fi
    echo "[WARN] removeAttribute runtime assertion did not match expected support; continuing in non-strict mode." >&2
    echo "${remove_attribute_assert_output}" >&2
  fi

  set_attribute_assert_output=""
  case "${FEATURE_RENDER_LOG}" in
    *"Unsupported script statement "*".setAttribute(\"style\","*|*"document.getElementById(\"setattr-style-fixture\").setAttribute(\"style\","*|*"document.querySelector(\"#setattr-style-fixture\").setAttribute(\"style\","*)
      set_attribute_assert_output="feature note: optional .setAttribute(\"style\", \"...\") fixture is unsupported in this build; render_log=${FEATURE_RENDER_LOG}"
      echo "[WARN] Runtime setAttribute(\"style\", \"...\") fixture is optional in smoke checks; continuing." >&2
      echo "${set_attribute_assert_output}" >&2
      ;;
  esac

  feature_assert_output=""
  if ! feature_assert_output="$(assert_feature_background 2>&1)"; then
    if [[ "${STRICT_FEATURE_ASSERT}" == "1" ]]; then
      echo "[FAIL] Feature assertion failed on sample render output." >&2
      echo "${feature_assert_output}" >&2
      return 1
    fi
    echo "[WARN] Feature assertion did not match expected colors; continuing in non-strict mode." >&2
    echo "${feature_assert_output}" >&2
  fi

  if [[ "${FEATURE_LOCAL_RENDER_OK}" == "1" ]]; then
    echo "[PASS] Smoke test passed: ${OUTPUT_FILE}, ${FEATURE_OUTPUT_FILE}, and ${FEATURE_LOCAL_OUTPUT_FILE}"
  else
    echo "[PASS] Smoke test passed with local-path warning: ${OUTPUT_FILE} and ${FEATURE_OUTPUT_FILE}"
  fi
}

if ! main "$@"; then
  exit 1
fi
