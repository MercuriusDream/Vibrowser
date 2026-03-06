#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=.codex/orchestrator/lib.sh
source "$SCRIPT_DIR/lib.sh"

MAX_CYCLES=0
BOOTSTRAP_ONLY=0
NO_SLEEP=0

usage() {
  cat <<'EOF'
Usage: bash .codex/orchestrator/supervisor.sh [--max-cycles N] [--bootstrap-only] [--no-sleep]
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --max-cycles)
      MAX_CYCLES="$2"
      shift
      ;;
    --bootstrap-only)
      BOOTSTRAP_ONLY=1
      ;;
    --no-sleep)
      NO_SLEEP=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
  shift
done

ensure_layout
seed_local_protocols
bootstrap_ledgers
init_state_if_missing
sync_shadow_ledger

if [[ $BOOTSTRAP_ONLY -eq 1 ]]; then
  echo "Codex estate supervisor bootstrap complete."
  exit 0
fi

run_verifier() {
  local cycle_dir=$1
  local verify_log="$cycle_dir/verifier.log"
  local verify_json="$cycle_dir/verifier.json"

  if "$TOOLS_DIR/pr-ready.sh" > "$verify_log" 2>&1; then
    cat > "$verify_json" <<EOF
{"status":"passed","report":"${verify_log#$PROJECT_DIR/}"}
EOF
    return 0
  fi

  cat > "$verify_json" <<EOF
{"status":"failed","report":"${verify_log#$PROJECT_DIR/}"}
EOF
  return 1
}

run_git_pr() {
  local cycle=$1
  local cycle_dir=$2
  local summary=$3
  "$TOOLS_DIR/pr.sh" --cycle "$cycle" --summary "$summary" > "$cycle_dir/git-pr.json" 2> "$cycle_dir/git-pr.log"
}

run_ci_wait() {
  local cycle_dir=$1
  local branch=""
  local pr_url=""
  local -a cmd

  if [[ -f "$cycle_dir/git-pr.json" ]]; then
    branch="$(sed -n '/^{/,$p' "$cycle_dir/git-pr.json" | jq -r '.branch // empty' 2>/dev/null || true)"
    pr_url="$(sed -n '/^{/,$p' "$cycle_dir/git-pr.json" | jq -r '.pr_url // empty' 2>/dev/null || true)"
  fi

  cmd=("$TOOLS_DIR/ci-wait.sh")
  if [[ -n "$branch" ]]; then
    cmd+=(--branch "$branch")
  fi
  if [[ -n "$pr_url" ]]; then
    cmd+=(--pr-url "$pr_url")
  fi

  "${cmd[@]}" > "$cycle_dir/ci.json" 2> "$cycle_dir/ci.log"
}

planner_prompt() {
  local cycle=$1
  cat <<EOF
You are the main orchestrator for Codex Estate Cycle $cycle.

Read these files first:
- .codex/codex-estate.local.md
- .codex/codex-estate.md
- .claude/claude-estate.md
- .codex/orchestrator/ultra-long-horizon-workload-map.md
- .codex/orchestrator/phase16-master-checklist.md
- .codex/orchestrator/state/current_cycle.json
- .codex/orchestrator/state/last_verify.json

Produce exactly 6 atomic workloads with disjoint owned paths.

Rules:
- You are the planner only in this step.
- Bias toward the highest-leverage browser-engine work in the ledger.
- If fewer than 6 obvious feature tasks exist, use debt-paydown or verification tasks to fill the remaining slots.
- Each workload must be implementable by one worker in one cycle.
- Output only schema-valid JSON.
EOF
}

integrator_prompt() {
  local cycle=$1
  local cycle_dir=$2
  cat <<EOF
You are the main orchestrator acting as integrator for Codex Estate Cycle $cycle.

Read:
- .codex/codex-estate.local.md
- .codex/codex-estate.md
- .codex/orchestrator/ultra-long-horizon-workload-map.md
- .codex/orchestrator/phase16-master-checklist.md
- $cycle_dir/plan.json
- $cycle_dir/worker-1.json
- $cycle_dir/worker-2.json
- $cycle_dir/worker-3.json
- $cycle_dir/worker-4.json
- $cycle_dir/worker-5.json
- $cycle_dir/worker-6.json

Inspect the repository diff and integrate all worker outputs into a coherent tree.

Rules:
- Resolve conflicts conservatively.
- Do not revert correct worker changes.
- Use shared-file edits only when integration requires them.
- Leave the tree ready for clean rebuild verification.
- Output only schema-valid JSON.
EOF
}

fixer_prompt() {
  local cycle=$1
  local cycle_dir=$2
  cat <<EOF
You are the main orchestrator acting as fixer for Codex Estate Cycle $cycle.

Read:
- .codex/codex-estate.local.md
- .codex/codex-estate.md
- .codex/orchestrator/ultra-long-horizon-workload-map.md
- .codex/orchestrator/phase16-master-checklist.md
- $cycle_dir/plan.json
- $cycle_dir/integrator.json
- $cycle_dir/verifier.log

Fix the smallest coherent set of issues needed to make verification pass.

Rules:
- Focus only on verifier.log failures.
- Do not start new feature work.
- Keep fixes small and directly tied to the reported failures.
- Output only schema-valid JSON.
EOF
}

ci_fixer_prompt() {
  local cycle=$1
  local cycle_dir=$2
  cat <<EOF
You are the main orchestrator acting as CI fixer for Codex Estate Cycle $cycle.

Read:
- .codex/codex-estate.local.md
- .codex/codex-estate.md
- .codex/orchestrator/ultra-long-horizon-workload-map.md
- .codex/orchestrator/phase16-master-checklist.md
- $cycle_dir/plan.json
- $cycle_dir/integrator.json
- $cycle_dir/ci.log
- $cycle_dir/ci.json

Goal:
- Fix the smallest coherent set of issues needed to satisfy CI after local verification already passed.

Rules:
- Do not start new feature work.
- Focus only on CI-reported failures.
- Keep fixes minimal and rerunnable.
- Output only schema-valid JSON.
EOF
}

summarize_cycle() {
  local cycle_dir=$1
  jq -r '[.workloads[]?.title // empty] | if length == 0 then "No workloads recorded" else join(" | ") end' "$cycle_dir/plan.json"
}

should_append_cycle_log() {
  local summary=$1
  [[ -n "$summary" && "$summary" != "n/a" && "$summary" != "No workloads recorded" ]]
}

cycle_counter=0
while true; do
  cycle_counter=$((cycle_counter + 1))
  if [[ $MAX_CYCLES -gt 0 && $cycle_counter -gt $MAX_CYCLES ]]; then
    break
  fi

  if [[ -f "$STOP_FILE" ]]; then
    echo "Stop file detected: $STOP_FILE"
    break
  fi

  cycle="$(next_cycle_number)"
  cycle_dir="$RUNS_DIR/cycle-$cycle"
  mkdir -p "$cycle_dir"
  ln -sfn "$cycle_dir" "$LOG_DIR/current"

  update_state "$cycle" "planner" "running" "planner" "Selecting 6 workloads"
  while ! codex_exec_role "planner" \
      "$SCHEMA_DIR/planner.schema.json" \
      "$cycle_dir/plan.json" \
      "$cycle_dir/planner.log" \
      "$(planner_prompt "$cycle")" \
      "$MAIN_MODEL" \
      "$MAIN_REASONING" \
      "$MAIN_FAST_FLAG"; do
    update_state "$cycle" "planner" "retrying" "planner" "Planner failed, retrying same cycle"
    [[ $NO_SLEEP -eq 0 ]] && sleep "$ERROR_SLEEP"
    if [[ -f "$STOP_FILE" ]]; then
      echo "Stop file detected: $STOP_FILE"
      exit 0
    fi
  done

  cp "$cycle_dir/plan.json" "$LAST_PLAN_FILE"
  update_state "$cycle" "workers" "running" "workers" "Running 6 external Codex workers"

  worker_pids=()
  for slot in $(seq 1 "$WORKER_COUNT"); do
    bash "$SCRIPT_DIR/worker.sh" "$cycle" "$slot" > "$cycle_dir/worker-$slot.launch.log" 2>&1 &
    worker_pids+=($!)
    sleep 1
  done

  workers_failed=0
  for pid in "${worker_pids[@]}"; do
    if ! wait "$pid"; then
      workers_failed=1
    fi
  done

  update_state "$cycle" "integrator" "running" "integrator" "Integrating worker results"
  while ! codex_exec_role "integrator" \
      "$SCHEMA_DIR/integrator.schema.json" \
      "$cycle_dir/integrator.json" \
      "$cycle_dir/integrator.log" \
      "$(integrator_prompt "$cycle" "$cycle_dir")" \
      "$MAIN_MODEL" \
      "$MAIN_REASONING" \
      "$MAIN_FAST_FLAG"; do
    update_state "$cycle" "integrator" "retrying" "integrator" "Integrator failed, retrying same cycle"
    [[ $NO_SLEEP -eq 0 ]] && sleep "$ERROR_SLEEP"
    if [[ -f "$STOP_FILE" ]]; then
      echo "Stop file detected: $STOP_FILE"
      exit 0
    fi
  done

  update_state "$cycle" "verifier" "running" "verifier" "Running clean build/test/bench/smoke gate"
  verify_ok=0
  repair_iteration=0
  git_pr_status=""
  ci_status=""
  while true; do
    if run_verifier "$cycle_dir"; then
      verify_ok=1
      break
    fi

    repair_iteration=$((repair_iteration + 1))
    update_state "$cycle" "fixer" "running" "fixer" "Verification failed, repair iteration $repair_iteration"

    while ! codex_exec_role "fixer" \
        "$SCHEMA_DIR/fixer.schema.json" \
        "$cycle_dir/fixer.json" \
        "$cycle_dir/fixer.log" \
        "$(fixer_prompt "$cycle" "$cycle_dir")" \
        "$MAIN_MODEL" \
        "$MAIN_REASONING" \
        "$MAIN_FAST_FLAG"; do
      update_state "$cycle" "fixer" "retrying" "fixer" "Fixer invocation failed, retrying repair iteration $repair_iteration"
      [[ $NO_SLEEP -eq 0 ]] && sleep "$ERROR_SLEEP"
      if [[ -f "$STOP_FILE" ]]; then
        echo "Stop file detected: $STOP_FILE"
        exit 0
      fi
    done

    update_state "$cycle" "verifier" "running" "verifier" "Re-running verification after repair iteration $repair_iteration"
    [[ $NO_SLEEP -eq 0 ]] && sleep 1
  done

  cp "$cycle_dir/verifier.json" "$LAST_VERIFY_FILE"
  summary="$(summarize_cycle "$cycle_dir")"

  if [[ $workers_failed -eq 0 && $verify_ok -eq 1 && "$AUTO_GIT" == "1" ]]; then
    update_state "$cycle" "git" "running" "git" "Preparing branch/commit/PR"
    if ! run_git_pr "$cycle" "$cycle_dir" "$summary"; then
      workers_failed=1
    elif [[ -f "$cycle_dir/git-pr.json" ]]; then
      git_pr_status="$(jq -r '.status // empty' < "$cycle_dir/git-pr.json" 2>/dev/null || true)"
    fi
  fi

  if [[ $workers_failed -eq 0 && $verify_ok -eq 1 && "$AUTO_CI_WAIT" == "1" ]]; then
    if [[ "$git_pr_status" == "no_changes" ]]; then
      cat > "$cycle_dir/ci.json" <<EOF
{"status":"skipped_no_changes","branch":"","pr_url":""}
EOF
      update_state "$cycle" "ci" "skipped" "ci" "Skipping CI wait because no changes were committed"
    else
      update_state "$cycle" "ci" "running" "ci" "Waiting for CI checks"
      while ! run_ci_wait "$cycle_dir"; do
        ci_status="$(jq -r '.status // empty' < "$cycle_dir/ci.json" 2>/dev/null || true)"
        if [[ "$ci_status" == "no_pr" || "$ci_status" == "no_checks" ]]; then
          update_state "$cycle" "ci" "skipped" "ci" "Skipping CI fixer because no PR or checks were created for this cycle"
          break
        fi

        if [[ "$AUTO_CI_FIX" != "1" ]]; then
          workers_failed=1
          break
        fi

        update_state "$cycle" "ci-fixer" "running" "ci-fixer" "CI failed, running same-cycle CI fixer"
        while ! codex_exec_role "ci-fixer" \
            "$SCHEMA_DIR/fixer.schema.json" \
            "$cycle_dir/ci-fixer.json" \
            "$cycle_dir/ci-fixer.log" \
            "$(ci_fixer_prompt "$cycle" "$cycle_dir")" \
            "$MAIN_MODEL" \
            "$MAIN_REASONING" \
            "$MAIN_FAST_FLAG"; do
          update_state "$cycle" "ci-fixer" "retrying" "ci-fixer" "CI fixer invocation failed, retrying same cycle"
          [[ $NO_SLEEP -eq 0 ]] && sleep "$ERROR_SLEEP"
          if [[ -f "$STOP_FILE" ]]; then
            echo "Stop file detected: $STOP_FILE"
            exit 0
          fi
        done

        while ! run_verifier "$cycle_dir"; do
          repair_iteration=$((repair_iteration + 1))
          update_state "$cycle" "fixer" "running" "fixer" "Post-CI repair iteration $repair_iteration"
          while ! codex_exec_role "fixer" \
              "$SCHEMA_DIR/fixer.schema.json" \
              "$cycle_dir/fixer.json" \
              "$cycle_dir/fixer.log" \
              "$(fixer_prompt "$cycle" "$cycle_dir")" \
              "$MAIN_MODEL" \
              "$MAIN_REASONING" \
              "$MAIN_FAST_FLAG"; do
            update_state "$cycle" "fixer" "retrying" "fixer" "Post-CI fixer invocation failed, retrying repair iteration $repair_iteration"
            [[ $NO_SLEEP -eq 0 ]] && sleep "$ERROR_SLEEP"
            if [[ -f "$STOP_FILE" ]]; then
              echo "Stop file detected: $STOP_FILE"
              exit 0
            fi
          done
        done

        cp "$cycle_dir/verifier.json" "$LAST_VERIFY_FILE"
        if [[ "$AUTO_GIT" == "1" ]]; then
          run_git_pr "$cycle" "$cycle_dir" "$summary" || workers_failed=1
          if [[ -f "$cycle_dir/git-pr.json" ]]; then
            git_pr_status="$(jq -r '.status // empty' < "$cycle_dir/git-pr.json" 2>/dev/null || true)"
            if [[ "$git_pr_status" == "no_changes" ]]; then
              cat > "$cycle_dir/ci.json" <<EOF
{"status":"skipped_no_changes","branch":"","pr_url":""}
EOF
              update_state "$cycle" "ci" "skipped" "ci" "Skipping CI wait because same-cycle repair produced no new changes"
              break
            fi
          fi
        fi
        update_state "$cycle" "ci" "running" "ci" "Re-waiting for CI after same-cycle repair"
      done
    fi
  fi

  update_state "$cycle" "ledger" "$([[ $verify_ok -eq 1 ]] && echo passed || echo failed)" "ledger" "$summary"
  if should_append_cycle_log "$summary"; then
    append_supervisor_log "$cycle" "$summary" "$cycle_dir/plan.json" "$cycle_dir/verifier.json"
  fi
  update_ledger_metadata "$cycle" "$summary"
  sync_shadow_ledger

  if [[ $workers_failed -eq 0 && $verify_ok -eq 1 ]]; then
    update_state "$cycle" "idle" "passed" "" "$summary"
  else
    update_state "$cycle" "idle" "failed" "" "$summary"
  fi

  if [[ $NO_SLEEP -eq 0 ]]; then
    sleep "$SLEEP_BETWEEN"
  fi
done

echo "Codex estate supervisor stopped."
