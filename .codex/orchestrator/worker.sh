#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=.codex/orchestrator/lib.sh
source "$SCRIPT_DIR/lib.sh"

if [[ $# -ne 2 ]]; then
  echo "Usage: bash .codex/orchestrator/worker.sh <cycle> <slot>" >&2
  exit 1
fi

cycle="$1"
slot="$2"
cycle_dir="$RUNS_DIR/cycle-$cycle"
plan_path="$cycle_dir/plan.json"
worker_name="worker-$slot"
output_path="$cycle_dir/$worker_name.json"
log_path="$cycle_dir/$worker_name.log"

if [[ ! -f "$plan_path" ]]; then
  echo "Missing plan file: $plan_path" >&2
  exit 1
fi

workload_json="$(jq ".workloads[$((slot - 1))]" "$plan_path")"
if [[ "$workload_json" == "null" ]]; then
  cat > "$output_path" <<EOF
{"status":"skipped","summary":"No workload assigned to slot $slot","changed_paths":[],"tests_run":[],"blockers":[]}
EOF
  exit 0
fi

title="$(jq -r '.title' <<<"$workload_json")"
objective="$(jq -r '.objective' <<<"$workload_json")"
verification="$(jq -r '.verification' <<<"$workload_json")"
prompt_seed="$(jq -r '.prompt_seed' <<<"$workload_json")"
owned_paths="$(jq -r '.owned_paths[]' <<<"$workload_json")"

prompt=$(
  cat <<EOF
You are $worker_name for Codex Estate Cycle $cycle.

Read:
- .codex/codex-estate.local.md
- .codex/codex-estate.md
- .codex/orchestrator/ultra-long-horizon-workload-map.md
- .codex/orchestrator/phase16-master-checklist.md
- $plan_path

Assigned workload:
- Title: $title
- Objective: $objective
- Verification target: $verification

Owned paths:
$owned_paths

Rules:
- Touch only owned paths unless a small adjacent fix is absolutely required.
- Do not edit estate ledgers.
- Implement the workload completely enough for integration.
- Run only focused verification relevant to your change when practical.
- You are a fast worker executing a quantized task from the main orchestrator.
- Output only schema-valid JSON.

Prompt seed:
$prompt_seed
EOF
)

codex_exec_role "$worker_name" \
  "$SCHEMA_DIR/worker.schema.json" \
  "$output_path" \
  "$log_path" \
  "$prompt" \
  "$SUBAGENT_MODEL" \
  "$SUBAGENT_REASONING" \
  "$SUBAGENT_FAST_FLAG"
