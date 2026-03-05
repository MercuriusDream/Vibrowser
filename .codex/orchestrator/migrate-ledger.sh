#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=.codex/orchestrator/lib.sh
source "$SCRIPT_DIR/lib.sh"

ensure_layout
seed_local_protocols

if [[ -f "$SHADOW_LEDGER" ]]; then
  cp "$SHADOW_LEDGER" "$CANONICAL_LEDGER"
fi

bootstrap_ledgers
init_state_if_missing
sync_shadow_ledger

cycle="$(ledger_cycle)"
if [[ -z "${cycle:-}" ]]; then
  cycle=0
fi

update_state "$cycle" "idle" "migrated" "" "Migrated Claude estate into canonical Codex ledger"

echo "Migration complete."
echo "Canonical ledger: $CANONICAL_LEDGER"
echo "Shadow ledger:    $SHADOW_LEDGER"
echo "Cycle:            $cycle"
