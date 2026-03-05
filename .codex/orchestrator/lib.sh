#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ORCH_DIR="$PROJECT_DIR/.codex/orchestrator"
STATE_DIR="$ORCH_DIR/state"
SESSIONS_DIR="$STATE_DIR/sessions"
RUNS_DIR="$STATE_DIR/runs"
LOG_DIR="$ORCH_DIR/logs"
SCHEMA_DIR="$ORCH_DIR/schemas"
TOOLS_DIR="$PROJECT_DIR/tools/codex"
WORKLOAD_MAP="$ORCH_DIR/ultra-long-horizon-workload-map.md"
CANONICAL_LEDGER="$PROJECT_DIR/.codex/codex-estate.md"
SHADOW_LEDGER="$PROJECT_DIR/.claude/claude-estate.md"
CODEX_LOCAL_PROTOCOL="$PROJECT_DIR/.codex/codex-estate.local.md"
CLAUDE_LOCAL_PROTOCOL="$PROJECT_DIR/.claude/claude-estate.local.md"
STOP_FILE="$PROJECT_DIR/.claude/.claude-estate-stop"
CURRENT_STATE_FILE="$STATE_DIR/current_cycle.json"
LAST_PLAN_FILE="$STATE_DIR/last_plan.json"
LAST_VERIFY_FILE="$STATE_DIR/last_verify.json"
MAIN_MODEL="${CODEX_ESTATE_MAIN_MODEL:-gpt-5.4}"
MAIN_REASONING="${CODEX_ESTATE_MAIN_REASONING:-high}"
MAIN_FAST_FLAG="${CODEX_ESTATE_MAIN_FAST_FLAG:-1}"
SUBAGENT_MODEL="${CODEX_ESTATE_SUBAGENT_MODEL:-gpt-5.4}"
SUBAGENT_REASONING="${CODEX_ESTATE_SUBAGENT_REASONING:-medium}"
SUBAGENT_FAST_FLAG="${CODEX_ESTATE_SUBAGENT_FAST_FLAG:-1}"
WORKER_COUNT="${CODEX_ESTATE_WORKERS:-6}"
SLEEP_BETWEEN="${CODEX_ESTATE_SLEEP_BETWEEN:-10}"
ERROR_SLEEP="${CODEX_ESTATE_ERROR_SLEEP:-30}"
MAX_ERRORS="${CODEX_ESTATE_MAX_ERRORS:-3}"
TIMEOUT_SECONDS="${CODEX_ESTATE_TIMEOUT_SECONDS:-1800}"
TMUX_SESSION="${CODEX_ESTATE_TMUX_SESSION:-vibrowser-estate}"
AUTO_GIT="${CODEX_ESTATE_AUTOGIT:-0}"
AUTO_PUSH="${CODEX_ESTATE_PUSH:-0}"
AUTO_PR="${CODEX_ESTATE_CREATE_PR:-0}"
AUTO_CI_WAIT="${CODEX_ESTATE_WAIT_FOR_CI:-0}"
AUTO_CI_FIX="${CODEX_ESTATE_CI_AUTOFIX:-0}"

ensure_layout() {
  mkdir -p "$STATE_DIR" "$SESSIONS_DIR" "$RUNS_DIR" "$LOG_DIR" "$SCHEMA_DIR" "$TOOLS_DIR"
}

ensure_file() {
  local path=$1
  local content=${2:-}
  if [[ ! -f "$path" ]]; then
    printf "%s" "$content" > "$path"
  fi
}

extract_session_id() {
  local file=$1
  local value

  value=$(grep -oE '\"id\"[[:space:]]*:[[:space:]]*\"[a-f0-9-]{36}\"' "$file" 2>/dev/null | head -n 1 || true)
  if [[ -n "$value" ]]; then
    echo "$value" | grep -oE '[a-f0-9-]{36}'
    return
  fi

  value=$(grep -oE 'session_id[[:space:]:=\"\\]*[a-f0-9-]{36}' "$file" 2>/dev/null | tail -n 1 || true)
  if [[ -n "$value" ]]; then
    echo "$value" | grep -oE '[a-f0-9-]{36}' | tail -n 1
  fi
}

json_escape() {
  printf '%s' "$1" | jq -Rsa .
}

ledger_cycle() {
  if [[ -f "$CURRENT_STATE_FILE" ]]; then
    jq -r '.cycle // empty' "$CURRENT_STATE_FILE" 2>/dev/null || true
  elif [[ -f "$CANONICAL_LEDGER" ]]; then
    awk -F': ' '/^\*\*Cycle\*\*: / {print $2; exit}' "$CANONICAL_LEDGER" 2>/dev/null || true
  fi
}

init_state_if_missing() {
  ensure_layout
  local cycle
  cycle=$(ledger_cycle)
  if [[ -z "${cycle:-}" ]]; then
    cycle=0
  fi
  ensure_file "$CURRENT_STATE_FILE" "$(cat <<EOF
{
  "cycle": $cycle,
  "phase": "idle",
  "status": "ready",
  "active_role": "",
  "last_summary": "",
  "updated_at": "$(date '+%Y-%m-%dT%H:%M:%S%z')"
}
EOF
)"
  ensure_file "$LAST_PLAN_FILE" '{"workloads":[]}'
  ensure_file "$LAST_VERIFY_FILE" '{"status":"unknown"}'
}

update_state() {
  local cycle=$1
  local phase=$2
  local status=$3
  local active_role=$4
  local summary=$5
  cat > "$CURRENT_STATE_FILE" <<EOF
{
  "cycle": $cycle,
  "phase": $(json_escape "$phase"),
  "status": $(json_escape "$status"),
  "active_role": $(json_escape "$active_role"),
  "last_summary": $(json_escape "$summary"),
  "updated_at": "$(date '+%Y-%m-%dT%H:%M:%S%z')"
}
EOF
}

sync_shadow_ledger() {
  if [[ -f "$CANONICAL_LEDGER" ]]; then
    cp "$CANONICAL_LEDGER" "$SHADOW_LEDGER"
  fi
  if [[ -f "$CODEX_LOCAL_PROTOCOL" ]]; then
    cp "$CODEX_LOCAL_PROTOCOL" "$CLAUDE_LOCAL_PROTOCOL"
  fi
}

next_cycle_number() {
  local current
  current=$(jq -r '.cycle // 0' "$CURRENT_STATE_FILE")
  echo $((current + 1))
}

role_session_file() {
  local role=$1
  echo "$SESSIONS_DIR/$role.session"
}

codex_exec_role() {
  local role=$1
  local schema_path=$2
  local output_path=$3
  local log_path=$4
  local prompt=$5
  local model=$6
  local reasoning=$7
  local fast_mode=${8:-0}
  local session_file
  local -a cmd

  session_file="$(role_session_file "$role")"
  cmd=(
    codex exec
    --json
    --cd "$PROJECT_DIR"
    --skip-git-repo-check
    --dangerously-bypass-approvals-and-sandbox
    --model "$model"
    -c "model_reasoning_effort=\"$reasoning\""
    -o "$output_path"
  )

  if [[ "$fast_mode" == "1" ]]; then
    cmd+=(--enable fast_mode)
  fi

  if [[ -n "$schema_path" ]]; then
    cmd+=(--output-schema "$schema_path")
  fi

  if [[ -s "$session_file" ]]; then
    cmd+=(resume "$(cat "$session_file")" "$prompt")
  else
    cmd+=("$prompt")
  fi

  if command -v timeout >/dev/null 2>&1; then
    timeout "$TIMEOUT_SECONDS" "${cmd[@]}" > "$log_path" 2>&1
  else
    "${cmd[@]}" > "$log_path" 2>&1
  fi

  local new_session
  new_session="$(extract_session_id "$log_path" || true)"
  if [[ -n "$new_session" ]]; then
    printf "%s" "$new_session" > "$session_file"
  fi
}

append_supervisor_log() {
  local cycle=$1
  local summary=$2
  local plan_path=$3
  local verify_path=$4

  if ! grep -q '^## Codex CLI Supervisor Log$' "$CANONICAL_LEDGER"; then
    printf "\n## Codex CLI Supervisor Log\n" >> "$CANONICAL_LEDGER"
  fi

  {
    printf "\n### Cycle %s — %s\n" "$cycle" "$(date '+%Y-%m-%d %H:%M:%S %z')"
    printf -- "- Runtime: main `%s/%s` (fast=%s), workers `%s/%s` (fast=%s)\n" \
      "$MAIN_MODEL" "$MAIN_REASONING" "$MAIN_FAST_FLAG" "$SUBAGENT_MODEL" "$SUBAGENT_REASONING" "$SUBAGENT_FAST_FLAG"
    printf -- "- Summary: %s\n" "$summary"
    printf -- "- Planner output: `%s`\n" "${plan_path#$PROJECT_DIR/}"
    printf -- "- Verification report: `%s`\n" "${verify_path#$PROJECT_DIR/}"
  } >> "$CANONICAL_LEDGER"
}

update_ledger_metadata() {
  local cycle=$1
  local summary=$2
  local tmp_file
  tmp_file="$(mktemp "${CANONICAL_LEDGER}.tmp.XXXXXX")"

  awk -v last_active="$(date '+%Y-%m-%d')" \
      -v cycle="$cycle" \
      -v focus="Cycle ${cycle} — ${summary}" '
    /^\*\*Last Active\*\*: / {print "**Last Active**: " last_active; next}
    /^\*\*Current Focus\*\*: / {print "**Current Focus**: " focus; next}
    /^\*\*Cycle\*\*: / {print "**Cycle**: " cycle; next}
    /^\*\*Workflow\*\*: / {
      print "**Workflow**: Codex CLI ultra-long-horizon orchestration. Planner -> 6 external Codex workers -> integrator -> verifier -> fixer -> optional git/PR/CI loop -> ledger sync. tmux is the primary live observability surface."
      next
    }
    {print}
  ' "$CANONICAL_LEDGER" > "$tmp_file"
  mv "$tmp_file" "$CANONICAL_LEDGER"
}

seed_local_protocols() {
  ensure_file "$CODEX_LOCAL_PROTOCOL" "$(cat <<EOF
# Codex Estate Local Protocol

- Canonical ledger: \`.codex/codex-estate.md\`
- Shadow compatibility ledger: \`.claude/claude-estate.md\`
- Main orchestrator runtime: \`${MAIN_MODEL}\` with reasoning \`${MAIN_REASONING}\`
- Main orchestrator fast mode: \`${MAIN_FAST_FLAG}\`
- Worker runtime: \`${SUBAGENT_MODEL}\` with reasoning \`${SUBAGENT_REASONING}\`
- Worker fast mode: \`${SUBAGENT_FAST_FLAG}\`
- Each cycle must plan exactly 6 atomic workloads when feasible.
- Each workload must declare disjoint owned paths before implementation.
- External orchestration mode is authoritative: planner -> worker-1..worker-6 -> integrator -> verifier -> fixer (at most one round) -> ledger.
- Planner must read \`.codex/orchestrator/ultra-long-horizon-workload-map.md\` every cycle.
- Planner must read \`.codex/orchestrator/phase16-master-checklist.md\` every cycle.
- Workers do not commit or push directly. Integration happens after all workers finish.
- After worker rounds, always do a clean rebuild before final verification.
- Verification gate: \`tools/codex/pr-ready.sh\`
- Optional post-verify git/PR/CI loop is controlled by:
  - \`CODEX_ESTATE_AUTOGIT\`
  - \`CODEX_ESTATE_PUSH\`
  - \`CODEX_ESTATE_CREATE_PR\`
  - \`CODEX_ESTATE_WAIT_FOR_CI\`
  - \`CODEX_ESTATE_CI_AUTOFIX\`
- Stop file: \`.claude/.claude-estate-stop\`
- Human live view surface: \`.codex/orchestrator/tmux-launch.sh\`
- Fast launcher: \`.codex/orchestrator/start-full-autonomy-fast.sh\`
- zsh fast launcher: \`.codex/orchestrator/start-full-autonomy-fast.zsh\`
- launchd install: \`.codex/orchestrator/install-launchd.sh\`
EOF
)"

  ensure_file "$CLAUDE_LOCAL_PROTOCOL" "$(cat <<'EOF'
# Claude Estate Compatibility Note

- Canonical estate ledger has migrated to `.codex/codex-estate.md`.
- `.claude/claude-estate.md` is now a shadow copy for backward compatibility.
- The active orchestration runtime is Codex CLI with external supervisor/tmux control.
- If this file is read by older loops, they should not diverge from `.codex/codex-estate.md`.
EOF
)"
}

bootstrap_ledgers() {
  if [[ ! -f "$CANONICAL_LEDGER" && -f "$SHADOW_LEDGER" ]]; then
    cp "$SHADOW_LEDGER" "$CANONICAL_LEDGER"
  elif [[ ! -f "$CANONICAL_LEDGER" ]]; then
    cat > "$CANONICAL_LEDGER" <<'EOF'
# Codex Estate Ledger

## Current Status

**Phase**: Bootstrap
**Last Active**: 1970-01-01
**Current Focus**: Bootstrap
**Cycle**: 0
EOF
  fi

  sync_shadow_ledger
}
