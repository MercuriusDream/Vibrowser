#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

bash "$ROOT_DIR/tools/codex/build.sh"
bash "$ROOT_DIR/tools/codex/test.sh"
bash "$ROOT_DIR/tools/codex/bench.sh"
bash "$ROOT_DIR/tools/codex/smoke.sh"
